/* ide-clang-translation-unit.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "clang-translation-unit"

#include <clang-c/Index.h>
#include <dazzle.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "ide-clang-completion-item.h"
#include "ide-clang-completion-item-private.h"
#include "ide-clang-private.h"
#include "ide-clang-symbol-tree.h"
#include "ide-clang-translation-unit.h"

struct _IdeClangTranslationUnit
{
  IdeObject          parent_instance;

  IdeRefPtr         *native;
  gint64             serial;
  GFile             *file;
  IdeHighlightIndex *index;
  GHashTable        *diagnostics;
};

typedef struct
{
  GPtrArray *unsaved_files;
  gchar     *path;
  guint      line;
  guint      line_offset;
} CodeCompleteState;

typedef struct
{
  GPtrArray *ar;
  IdeFile   *file;
  gchar     *path;
} GetSymbolsState;

G_DEFINE_TYPE (IdeClangTranslationUnit, ide_clang_translation_unit, IDE_TYPE_OBJECT)
DZL_DEFINE_COUNTER (instances, "Clang", "Translation Units", "Number of clang translation units")

enum {
  PROP_0,
  PROP_FILE,
  PROP_INDEX,
  PROP_NATIVE,
  PROP_SERIAL,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
code_complete_state_free (gpointer data)
{
  CodeCompleteState *state = data;

  if (state)
    {
      g_clear_pointer (&state->unsaved_files, g_ptr_array_unref);
      g_free (state->path);
      g_free (state);
    }
}

static CXFile
get_file_for_location (IdeClangTranslationUnit *self,
                       IdeSourceLocation       *location)
{
  g_autofree gchar *filename = NULL;
  IdeFile *file;
  GFile *gfile;

  g_assert (IDE_IS_CLANG_TRANSLATION_UNIT (self));
  g_assert (location != NULL);

  if (!(file = ide_source_location_get_file (location)) ||
      !(gfile = ide_file_get_file (file)) ||
      !(filename = g_file_get_path (gfile)))
    return NULL;

  return clang_getFile (ide_ref_ptr_get (self->native), filename);
}

/**
 * ide_clang_translation_unit_get_index:
 * @self: an #IdeClangTranslationUnit.
 *
 * Gets the highlight index for the translation unit.
 *
 * Returns: (transfer none) (nullable): An #IdeHighlightIndex or %NULL.
 */
IdeHighlightIndex *
ide_clang_translation_unit_get_index (IdeClangTranslationUnit *self)
{
  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);

  return self->index;
}

static void
ide_clang_translation_unit_set_index (IdeClangTranslationUnit *self,
                                      IdeHighlightIndex       *index)
{
  g_assert (IDE_IS_CLANG_TRANSLATION_UNIT (self));

  if (index != NULL)
    self->index = ide_highlight_index_ref (index);
}

GFile *
ide_clang_translation_unit_get_file (IdeClangTranslationUnit *self)
{
  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);

  return self->file;
}

static void
ide_clang_translation_unit_set_file (IdeClangTranslationUnit *self,
                                     GFile                   *file)
{
  g_return_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self));
  g_return_if_fail (G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
}

IdeClangTranslationUnit *
_ide_clang_translation_unit_new (IdeContext        *context,
                                 CXTranslationUnit  tu,
                                 GFile             *file,
                                 IdeHighlightIndex *index,
                                 gint64             serial)
{
  IdeClangTranslationUnit *ret;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (tu, NULL);
  g_return_val_if_fail (!file || G_IS_FILE (file), NULL);

  ret = g_object_new (IDE_TYPE_CLANG_TRANSLATION_UNIT,
                      "context", context,
                      "file", file,
                      "index", index,
                      "native", tu,
                      "serial", serial,
                      NULL);

  return ret;
}

static IdeDiagnosticSeverity
translate_severity (enum CXDiagnosticSeverity severity)
{
  switch (severity)
    {
    case CXDiagnostic_Ignored:
      return IDE_DIAGNOSTIC_IGNORED;

    case CXDiagnostic_Note:
      return IDE_DIAGNOSTIC_NOTE;

    case CXDiagnostic_Warning:
      return IDE_DIAGNOSTIC_WARNING;

    case CXDiagnostic_Error:
      return IDE_DIAGNOSTIC_ERROR;

    case CXDiagnostic_Fatal:
      return IDE_DIAGNOSTIC_FATAL;

    default:
      return 0;
    }
}

static gchar *
get_path (const gchar *workpath,
          const gchar *path)
{
  if (path == NULL)
    return g_strdup (workpath);
  else if (g_str_has_prefix (path, workpath))
    return g_strdup (path);
  else
    return g_build_filename (workpath, path, NULL);
}

static IdeSourceLocation *
create_location (IdeClangTranslationUnit *self,
                 const gchar             *workpath,
                 CXSourceLocation         cxloc,
                 IdeSourceLocation       *alternate)
{
  g_autofree gchar *path = NULL;
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(GFile) gfile = NULL;
  g_auto(CXString) str = {0};
  IdeContext *context;
  CXFile cxfile = NULL;
  unsigned line;
  unsigned column;
  unsigned offset;

  g_assert (IDE_IS_CLANG_TRANSLATION_UNIT (self));
  g_assert (workpath != NULL);

  clang_getFileLocation (cxloc, &cxfile, &line, &column, &offset);

  str = clang_getFileName (cxfile);

  if (line == 0 || clang_getCString (str) == NULL)
    return alternate ? ide_source_location_ref (alternate) : NULL;

  if (line > 0)
    line--;

  if (column > 0)
    column--;

  path = get_path (workpath, clang_getCString (str));
  context = ide_object_get_context (IDE_OBJECT (self));
  gfile = g_file_new_for_path (path);
  file = ide_file_new (context, gfile);

  return ide_source_location_new (file, line, column, offset);
}

static IdeSourceRange *
create_range (IdeClangTranslationUnit *self,
              const gchar             *workpath,
              CXSourceRange            cxrange)
{
  IdeSourceRange *range = NULL;
  CXSourceLocation cxbegin;
  CXSourceLocation cxend;
  g_autoptr(IdeSourceLocation) begin = NULL;
  g_autoptr(IdeSourceLocation) end = NULL;

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);

  cxbegin = clang_getRangeStart (cxrange);
  cxend = clang_getRangeEnd (cxrange);

  /* Sometimes the end location does not have a file associated with it,
   * so we force it to have the IdeFile of the first location.
   */
  begin = create_location (self, workpath, cxbegin, NULL);
  end = create_location (self, workpath, cxend, begin);

  if ((begin != NULL) && (end != NULL))
    range = ide_source_range_new (begin, end);

  return range;
}

static gboolean
cxfile_equal (CXFile  cxfile,
              GFile  *file)
{
  g_auto(CXString) cxstr = {0};
  g_autofree gchar *path = NULL;
  const gchar *cstr;

  cxstr = clang_getFileName (cxfile);
  cstr = clang_getCString (cxstr);
  path = g_file_get_path (file);

  return dzl_str_equal0 (cstr, path);
}

static IdeDiagnostic *
create_diagnostic (IdeClangTranslationUnit *self,
                   const gchar             *workpath,
                   GFile                   *target,
                   CXDiagnostic            *cxdiag)
{
  g_autoptr(IdeSourceLocation) loc = NULL;
  enum CXDiagnosticSeverity cxseverity;
  IdeDiagnosticSeverity severity;
  IdeDiagnostic *diag;
  const gchar *spelling;
  g_auto(CXString) cxstr = {0};
  CXSourceLocation cxloc;
  CXFile cxfile = NULL;
  guint num_ranges;
  guint i;

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);
  g_return_val_if_fail (cxdiag, NULL);

  cxloc = clang_getDiagnosticLocation (cxdiag);
  clang_getExpansionLocation (cxloc, &cxfile, NULL, NULL, NULL);

  if (cxfile && !cxfile_equal (cxfile, target))
    return NULL;

  cxseverity = clang_getDiagnosticSeverity (cxdiag);
  severity = translate_severity (cxseverity);

  cxstr = clang_getDiagnosticSpelling (cxdiag);
  spelling = clang_getCString (cxstr);

  /*
   * I thought we could use an approach like the following to get deprecation
   * status. However, it has so far proven ineffective.
   *
   *   cursor = clang_getCursor (self->tu, cxloc);
   *   avail = clang_getCursorAvailability (cursor);
   */
  if ((severity == IDE_DIAGNOSTIC_WARNING) &&
      (spelling != NULL) &&
      (strstr (spelling, "deprecated") != NULL))
    severity = IDE_DIAGNOSTIC_DEPRECATED;

  loc = create_location (self, workpath, cxloc, NULL);

  diag = ide_diagnostic_new (severity, spelling, loc);

  num_ranges = clang_getDiagnosticNumRanges (cxdiag);

  for (i = 0; i < num_ranges; i++)
    {
      CXSourceRange cxrange;
      IdeSourceRange *range;

      cxrange = clang_getDiagnosticRange (cxdiag, i);
      range = create_range (self, workpath, cxrange);
      if (range != NULL)
        ide_diagnostic_take_range (diag, range);
    }

  return diag;
}

/**
 * ide_clang_translation_unit_get_diagnostics_for_file:
 *
 * Retrieves the diagnostics for the translation unit for a specific file.
 *
 * Returns: (transfer none) (nullable): An #IdeDiagnostics or %NULL.
 */
IdeDiagnostics *
ide_clang_translation_unit_get_diagnostics_for_file (IdeClangTranslationUnit *self,
                                                     GFile                   *file)
{
  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);

  if (!g_hash_table_contains (self->diagnostics, file))
    {
      CXTranslationUnit tu = ide_ref_ptr_get (self->native);
      g_autofree gchar *workpath = NULL;
      g_autoptr(GPtrArray) diags = NULL;
      IdeContext *context;
      GFile *workdir;
      IdeVcs *vcs;
      guint count;

      diags = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_diagnostic_unref);

      context = ide_object_get_context (IDE_OBJECT (self));
      vcs = ide_context_get_vcs (context);
      workdir = ide_vcs_get_working_directory (vcs);
      workpath = g_file_get_path (workdir);

      count = clang_getNumDiagnostics (tu);
      for (guint i = 0; i < count; i++)
        {
          g_autoptr(CXDiagnostic) cxdiag = NULL;
          g_autoptr(IdeDiagnostic) diag = NULL;

          cxdiag = clang_getDiagnostic (tu, i);
          diag = create_diagnostic (self, workpath, file, cxdiag);

          if (diag != NULL)
            {
              guint num_fixits = clang_getDiagnosticNumFixIts (cxdiag);

              for (guint j = 0; j < num_fixits; j++)
                {
                  g_autoptr(IdeSourceRange) range = NULL;
                  g_autoptr(IdeFixit) fixit = NULL;
                  g_auto(CXString) cxstr = {0};
                  CXSourceRange cxrange;

                  cxstr = clang_getDiagnosticFixIt (cxdiag, j, &cxrange);
                  range = create_range (self, workpath, cxrange);
                  fixit = ide_fixit_new (range, clang_getCString (cxstr));

                  if (fixit != NULL)
                    ide_diagnostic_take_fixit (diag, g_steal_pointer (&fixit));
                }

              g_ptr_array_add (diags, g_steal_pointer (&diag));
            }
        }

      g_hash_table_insert (self->diagnostics,
                           g_object_ref (file),
                           ide_diagnostics_new (g_steal_pointer (&diags)));
    }

  return g_hash_table_lookup (self->diagnostics, file);
}

/**
 * ide_clang_translation_unit_get_diagnostics:
 *
 * Retrieves the diagnostics for the translation unit.
 *
 * Returns: (transfer none) (nullable): An #IdeDiagnostics or %NULL.
 */
IdeDiagnostics *
ide_clang_translation_unit_get_diagnostics (IdeClangTranslationUnit *self)
{
  return ide_clang_translation_unit_get_diagnostics_for_file (self, self->file);
}

gint64
ide_clang_translation_unit_get_serial (IdeClangTranslationUnit *self)
{
  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), -1);

  return self->serial;
}

static void
ide_clang_translation_unit_set_native (IdeClangTranslationUnit *self,
                                       CXTranslationUnit        native)
{
  g_assert (IDE_IS_CLANG_TRANSLATION_UNIT (self));

  if (native != NULL)
    self->native = ide_ref_ptr_new (native, (GDestroyNotify)clang_disposeTranslationUnit);
}

static void
ide_clang_translation_unit_finalize (GObject *object)
{
  IdeClangTranslationUnit *self = (IdeClangTranslationUnit *)object;

  IDE_ENTRY;

  g_clear_pointer (&self->native, ide_ref_ptr_unref);
  g_clear_object (&self->file);
  g_clear_pointer (&self->index, ide_highlight_index_unref);
  g_clear_pointer (&self->diagnostics, g_hash_table_unref);

  G_OBJECT_CLASS (ide_clang_translation_unit_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);

  IDE_EXIT;
}

static void
ide_clang_translation_unit_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  IdeClangTranslationUnit *self = IDE_CLANG_TRANSLATION_UNIT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_clang_translation_unit_get_file (self));
      break;

    case PROP_INDEX:
      g_value_set_boxed (value, ide_clang_translation_unit_get_index (self));
      break;

    case PROP_SERIAL:
      g_value_set_int64 (value, ide_clang_translation_unit_get_serial (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_translation_unit_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  IdeClangTranslationUnit *self = IDE_CLANG_TRANSLATION_UNIT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      ide_clang_translation_unit_set_file (self, g_value_get_object (value));
      break;

    case PROP_INDEX:
      ide_clang_translation_unit_set_index (self, g_value_get_boxed (value));
      break;

    case PROP_SERIAL:
      self->serial = g_value_get_int64 (value);
      break;

    case PROP_NATIVE:
      ide_clang_translation_unit_set_native (self, g_value_get_pointer (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_translation_unit_class_init (IdeClangTranslationUnitClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_translation_unit_finalize;
  object_class->get_property = ide_clang_translation_unit_get_property;
  object_class->set_property = ide_clang_translation_unit_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The file used to build the translation unit.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_INDEX] =
    g_param_spec_boxed ("index",
                         "Index",
                         "The highlight index for the translation unit.",
                         IDE_TYPE_HIGHLIGHT_INDEX,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NATIVE] =
    g_param_spec_pointer ("native",
                          "Native",
                          "The native translation unit pointer.",
                          (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SERIAL] =
    g_param_spec_int64 ("serial",
                        "Serial",
                        "A sequence number for the translation unit.",
                        0,
                        G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_clang_translation_unit_init (IdeClangTranslationUnit *self)
{
  DZL_COUNTER_INC (instances);

  self->diagnostics = g_hash_table_new_full ((GHashFunc)g_file_hash,
                                             (GEqualFunc)g_file_equal,
                                             g_object_unref,
                                             (GDestroyNotify)ide_diagnostics_unref);
}

static void
ide_clang_translation_unit_code_complete_worker (GTask        *task,
                                                 gpointer      source_object,
                                                 gpointer      task_data,
                                                 GCancellable *cancellable)
{
  IdeClangTranslationUnit *self = source_object;
  CodeCompleteState *state = task_data;
  CXCodeCompleteResults *results;
  CXTranslationUnit tu;
  g_autoptr(IdeRefPtr) refptr = NULL;
  struct CXUnsavedFile *ufs;
  GPtrArray *ar;
  gsize i;
  gsize j = 0;

  g_assert (IDE_IS_CLANG_TRANSLATION_UNIT (self));
  g_assert (state);
  g_assert (state->unsaved_files);

  tu = ide_ref_ptr_get (self->native);

  /*
   * FIXME: Not thread safe! We should probably add a "Pending" flag or something that is
   *        similar to g_input_stream_set_pending().
   */

  if (!state->path)
    {
      /* implausable to reach here, anyway */
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               _("clang_codeCompleteAt() only works on local files"));
      return;
    }

  ufs = g_new0 (struct CXUnsavedFile, state->unsaved_files->len);

  for (i = 0; i < state->unsaved_files->len; i++)
    {
      IdeUnsavedFile *uf;
      gchar *path;
      GFile *file;

      uf = g_ptr_array_index (state->unsaved_files, i);
      file = ide_unsaved_file_get_file (uf);
      path = g_file_get_path (file);

      /*
       * NOTE: Some files might not be local, and therefore return a NULL path.
       *       Also, we will free the path from the (const char *) pointer after
       *       executing the work.
       */
      if (path != NULL)
        {
          GBytes *content = ide_unsaved_file_get_content (uf);

          ufs [j].Filename = path;
          ufs [j].Contents = g_bytes_get_data (content, NULL);
          ufs [j].Length = g_bytes_get_size (content);

          j++;
        }
    }

  results = clang_codeCompleteAt (tu,
                                  state->path,
                                  state->line + 1,
                                  state->line_offset + 1,
                                  ufs, j,
                                  clang_defaultCodeCompleteOptions ());

  /*
   * encapsulate in refptr so we don't need to malloc lots of little strings.
   * we will inflate result strings as necessary.
   */
  refptr = ide_ref_ptr_new (results, (GDestroyNotify)clang_disposeCodeCompleteResults);
  ar = g_ptr_array_new_with_free_func (g_object_unref);

  for (i = 0; i < results->NumResults; i++)
    g_ptr_array_add (ar, ide_clang_completion_item_new (refptr, i));

  g_task_return_pointer (task, ar, (GDestroyNotify)g_ptr_array_unref);

  /* cleanup malloc'd state */
  for (i = 0; i < j; i++)
    g_free ((gchar *)ufs [i].Filename);
  g_free (ufs);
}


void
ide_clang_translation_unit_code_complete_async (IdeClangTranslationUnit *self,
                                                GFile                   *file,
                                                const GtkTextIter       *location,
                                                GCancellable            *cancellable,
                                                GAsyncReadyCallback      callback,
                                                gpointer                 user_data)
{
  g_autoptr(GTask) task = NULL;
  CodeCompleteState *state;
  IdeContext *context;
  IdeUnsavedFiles *unsaved_files;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (location);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  unsaved_files = ide_context_get_unsaved_files (context);

  task = g_task_new (self, cancellable, callback, user_data);

  state = g_new0 (CodeCompleteState, 1);
  state->path = g_file_get_path (file);
  state->line = gtk_text_iter_get_line (location);
  state->line_offset = gtk_text_iter_get_line_offset (location);
  state->unsaved_files = ide_unsaved_files_to_array (unsaved_files);

  /*
   * TODO: Technically it is not safe for us to go run this in a thread. We need to ensure
   *       that only one thread is dealing with this at a time.
   */

  g_task_set_task_data (task, state, code_complete_state_free);

  ide_thread_pool_push_task (IDE_THREAD_POOL_COMPILER,
                             task,
                             ide_clang_translation_unit_code_complete_worker);

  IDE_EXIT;
}

/**
 * ide_clang_translation_unit_code_complete_finish:
 * @self: an #IdeClangTranslationUnit.
 * @result: a #GAsyncResult
 * @error: (out) (nullable): A location for a #GError, or %NULL.
 *
 * Completes a call to ide_clang_translation_unit_code_complete_async().
 *
 * Returns: (transfer container) (element-type GtkSourceCompletionProposal*): An array of
 *   #GtkSourceCompletionProposal. Upon failure, %NULL is returned.
 */
GPtrArray *
ide_clang_translation_unit_code_complete_finish (IdeClangTranslationUnit  *self,
                                                 GAsyncResult             *result,
                                                 GError                  **error)
{
  GTask *task = (GTask *)result;
  GPtrArray *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

static enum CXChildVisitResult
find_child_type (CXCursor     cursor,
                 CXCursor     parent,
                 CXClientData user_data)
{
  enum CXCursorKind *child_kind = user_data;
  enum CXCursorKind kind = clang_getCursorKind (cursor);

  switch ((int)kind)
    {
    case CXCursor_StructDecl:
    case CXCursor_UnionDecl:
    case CXCursor_EnumDecl:
      *child_kind = kind;
      return CXChildVisit_Break;

    case CXCursor_TypeRef:
      cursor = clang_getCursorReferenced (cursor);
      *child_kind = clang_getCursorKind (cursor);
      return CXChildVisit_Break;

    default:
      break;
    }

  return CXChildVisit_Continue;
}

static IdeSymbolKind
get_symbol_kind (CXCursor        cursor,
                 IdeSymbolFlags *flags)
{
  enum CXAvailabilityKind availability;
  enum CXCursorKind cxkind;
  IdeSymbolFlags local_flags = 0;
  IdeSymbolKind kind = 0;

  availability = clang_getCursorAvailability (cursor);
  if (availability == CXAvailability_Deprecated)
    local_flags |= IDE_SYMBOL_FLAGS_IS_DEPRECATED;

  cxkind = clang_getCursorKind (cursor);

  if (cxkind == CXCursor_TypedefDecl)
    {
      enum CXCursorKind child_kind = 0;

      clang_visitChildren (cursor, find_child_type, &child_kind);
      cxkind = child_kind;
    }

  switch ((int)cxkind)
    {
    case CXCursor_StructDecl:
      kind = IDE_SYMBOL_STRUCT;
      break;

    case CXCursor_UnionDecl:
      kind = IDE_SYMBOL_UNION;
      break;

    case CXCursor_ClassDecl:
      kind = IDE_SYMBOL_CLASS;
      break;

    case CXCursor_FunctionDecl:
      kind = IDE_SYMBOL_FUNCTION;
      break;

    case CXCursor_EnumDecl:
      kind = IDE_SYMBOL_ENUM;
      break;

    case CXCursor_EnumConstantDecl:
      kind = IDE_SYMBOL_ENUM_VALUE;
      break;

    case CXCursor_FieldDecl:
      kind = IDE_SYMBOL_FIELD;
      break;

    case CXCursor_InclusionDirective:
      kind = IDE_SYMBOL_HEADER;
      break;

    default:
      break;
    }

  *flags = local_flags;

  return kind;
}

IdeSymbol *
ide_clang_translation_unit_lookup_symbol (IdeClangTranslationUnit  *self,
                                          IdeSourceLocation        *location,
                                          GError                  **error)
{
  g_autofree gchar *workpath = NULL;
  g_auto(CXString) cxstr = { 0 };
  g_autoptr(IdeSourceLocation) declaration = NULL;
  g_autoptr(IdeSourceLocation) definition = NULL;
  g_autoptr(IdeSourceLocation) canonical = NULL;
  CXTranslationUnit tu;
  IdeSymbolKind symkind = 0;
  IdeSymbolFlags symflags = 0;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  CXSourceLocation cxlocation;
  CXCursor tmpcursor;
  CXCursor cursor;
  CXFile cxfile;
  IdeSymbol *ret = NULL;
  guint line;
  guint line_offset;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);
  g_return_val_if_fail (location != NULL, NULL);

  tu = ide_ref_ptr_get (self->native);

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  workpath = g_file_get_path (workdir);

  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  if (NULL == (cxfile = get_file_for_location (self, location)))
    IDE_RETURN (NULL);

  cxlocation = clang_getLocation (tu, cxfile, line + 1, line_offset + 1);
  cursor = clang_getCursor (tu, cxlocation);
  if (clang_Cursor_isNull (cursor))
    IDE_RETURN (NULL);

  tmpcursor = clang_getCursorDefinition (cursor);

  if (clang_Cursor_isNull (tmpcursor))
    tmpcursor = clang_getCursorReferenced (cursor);

  if (!clang_Cursor_isNull (tmpcursor))
    {
      CXSourceLocation tmploc;
      CXSourceRange cxrange;

      cxrange = clang_getCursorExtent (tmpcursor);
      tmploc = clang_getRangeStart (cxrange);

      if (clang_isCursorDefinition (tmpcursor))
        definition = create_location (self, workpath, tmploc, NULL);
      else
        declaration = create_location (self, workpath, tmploc, NULL);
    }

  symkind = get_symbol_kind (cursor, &symflags);

  if (symkind == IDE_SYMBOL_HEADER)
    {
      g_auto(CXString) included_file_name = {0};
      CXFile included_file;
      const gchar *path;

      included_file = clang_getIncludedFile (cursor);
      included_file_name = clang_getFileName (included_file);
      path = clang_getCString (included_file_name);

      if (path != NULL)
        {
          g_autoptr(IdeFile) file = NULL;
          g_autoptr(GFile) gfile = NULL;

          gfile = g_file_new_for_path (path);
          file = ide_file_new (context, gfile);

          g_clear_pointer (&definition, ide_symbol_unref);
          declaration = ide_source_location_new (file, 0, 0, 0);
        }
    }

  cxstr = clang_getCursorDisplayName (cursor);
  ret = ide_symbol_new (clang_getCString (cxstr), symkind, symflags,
                        declaration, definition, canonical);

  /*
   * TODO: We should also get information about the defintion of the symbol.
   *       Possibly more.
   */

  IDE_RETURN (ret);
}

static IdeSymbol *
create_symbol (CXCursor         cursor,
               GetSymbolsState *state)
{
  g_auto(CXString) cxname = {0};
  g_autoptr(IdeSourceLocation) srcloc = NULL;
  CXSourceLocation cxloc;
  IdeSymbolKind symkind;
  IdeSymbolFlags symflags;
  const gchar *name;
  guint line;
  guint line_offset;

  cxname = clang_getCursorSpelling (cursor);
  name = clang_getCString (cxname);
  cxloc = clang_getCursorLocation (cursor);
  clang_getFileLocation (cxloc, NULL, &line, &line_offset, NULL);
  srcloc = ide_source_location_new (state->file, line-1, line_offset-1, 0);
  symkind = get_symbol_kind (cursor, &symflags);

  return ide_symbol_new (name, symkind, symflags, NULL, NULL, srcloc);
}

static enum CXChildVisitResult
ide_clang_translation_unit_get_symbols__visitor_cb (CXCursor     cursor,
                                                    CXCursor     parent,
                                                    CXClientData user_data)
{
  GetSymbolsState *state = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_auto(CXString) filename = {0};
  CXSourceLocation cxloc;
  CXFile file;
  enum CXCursorKind kind;

  g_assert (state);

  cxloc = clang_getCursorLocation (cursor);
  clang_getFileLocation (cxloc, &file, NULL, NULL, NULL);
  filename = clang_getFileName (file);

  if (0 != g_strcmp0 (clang_getCString (filename), state->path))
    return CXChildVisit_Continue;

  kind = clang_getCursorKind (cursor);

  switch ((int)kind)
    {
    case CXCursor_FunctionDecl:
    case CXCursor_TypedefDecl:
      symbol = create_symbol (cursor, state);
      break;

    default:
      break;
    }

  if (symbol != NULL)
    g_ptr_array_add (state->ar, ide_symbol_ref (symbol));

  return CXChildVisit_Continue;
}

static gint
sort_symbols_by_name (gconstpointer a,
                      gconstpointer b)
{
  IdeSymbol **asym = (IdeSymbol **)a;
  IdeSymbol **bsym = (IdeSymbol **)b;

  return g_strcmp0 (ide_symbol_get_name (*asym),
                    ide_symbol_get_name (*bsym));
}

/**
 * ide_clang_translation_unit_get_symbols:
 *
 * Returns: (transfer container) (element-type Ide.Symbol): An array of #IdeSymbol.
 */
GPtrArray *
ide_clang_translation_unit_get_symbols (IdeClangTranslationUnit *self,
                                        IdeFile                 *file)
{
  GetSymbolsState state = { 0 };
  CXCursor cursor;

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);
  g_return_val_if_fail (IDE_IS_FILE (file), NULL);

  state.ar = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_symbol_unref);
  state.file = file;
  state.path = g_file_get_path (ide_file_get_file (file));

  cursor = clang_getTranslationUnitCursor (ide_ref_ptr_get (self->native));
  clang_visitChildren (cursor,
                       ide_clang_translation_unit_get_symbols__visitor_cb,
                       &state);

  g_ptr_array_sort (state.ar, sort_symbols_by_name);

  g_free (state.path);

  return state.ar;
}

void
ide_clang_translation_unit_get_symbol_tree_async (IdeClangTranslationUnit *self,
                                                  GFile                   *file,
                                                  GCancellable            *cancellable,
                                                  GAsyncReadyCallback      callback,
                                                  gpointer                 user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(IdeSymbolTree) symbol_tree = NULL;
  IdeContext *context;

  g_return_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_clang_translation_unit_get_symbol_tree_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  symbol_tree = g_object_new (IDE_TYPE_CLANG_SYMBOL_TREE,
                              "context", context,
                              "native", self->native,
                              "file", file,
                              NULL);
  g_task_return_pointer (task, g_steal_pointer (&symbol_tree), g_object_unref);
}

IdeSymbolTree *
ide_clang_translation_unit_get_symbol_tree_finish (IdeClangTranslationUnit  *self,
                                                   GAsyncResult             *result,
                                                   GError                  **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static gboolean
is_ignored_kind (enum CXCursorKind kind)
{
  switch ((int)kind)
    {
    case CXCursor_CXXMethod:
    case CXCursor_ClassDecl:
    case CXCursor_ClassTemplate:
    case CXCursor_Constructor:
    case CXCursor_Destructor:
    case CXCursor_EnumConstantDecl:
    case CXCursor_EnumDecl:
    case CXCursor_FunctionDecl:
    case CXCursor_FunctionTemplate:
    case CXCursor_Namespace:
    case CXCursor_NamespaceAlias:
    case CXCursor_StructDecl:
    case CXCursor_TranslationUnit:
    case CXCursor_TypeAliasDecl:
    case CXCursor_TypedefDecl:
    case CXCursor_UnionDecl:
      return FALSE;

    default:
      return TRUE;
    }
}

static CXCursor
move_to_previous_sibling (CXTranslationUnit unit,
                          CXCursor          cursor)
{
  CXSourceRange range = clang_getCursorExtent (cursor);
  CXSourceLocation begin = clang_getRangeStart (range);
  CXSourceLocation loc;
  CXFile file;
  unsigned line;
  unsigned column;

  clang_getFileLocation (begin, &file, &line, &column, NULL);
  loc = clang_getLocation (unit, file, line, column - 1);

  return clang_getCursor (unit, loc);
}

/**
 * ide_clang_translation_unit_find_nearest_scope:
 * @self: a #IdeClangTranslationUnit
 * @location: An #IdeSourceLocation within the unit
 * @error: A location for a #GError or %NULL
 *
 * This locates the nearest scope for @location and returns it
 * as an #IdeSymbol.
 *
 * Returns: (transfer full): An #IdeSymbol or %NULL and @error is set.
 *
 * Since: 3.26
 */
IdeSymbol *
ide_clang_translation_unit_find_nearest_scope (IdeClangTranslationUnit  *self,
                                               IdeSourceLocation        *location,
                                               GError                  **error)
{
  g_autoptr(IdeSourceLocation) symbol_location = NULL;
  g_auto(CXString) cxname = {0};
  CXTranslationUnit unit;
  CXSourceLocation loc;
  CXCursor cursor;
  enum CXCursorKind kind;
  IdeSymbolKind symkind;
  IdeSymbolFlags symflags;
  IdeSymbol *ret = NULL;
  CXFile file;
  IdeFile *ifile;
  guint line;
  guint line_offset;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);
  g_return_val_if_fail (location != NULL, NULL);

  ifile = ide_source_location_get_file (location);
  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  if (NULL == (file = get_file_for_location (self, location)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to locate file in translation unit");
      IDE_RETURN (ret);
    }

  unit = ide_ref_ptr_get (self->native);
  loc = clang_getLocation (unit, file, line + 1, line_offset + 1);
  cursor = clang_getCursor (unit, loc);

  if (clang_Cursor_isNull (cursor))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Location was not found in translation unit");
      IDE_RETURN (ret);
    }

  /*
   * Macros sort of mess us up and result in us thinking
   * we are in some sort of InvalidFile condition.
   */
  kind = clang_getCursorKind (cursor);
  if (kind == CXCursor_MacroExpansion)
    cursor = move_to_previous_sibling (unit, cursor);

  /*
   * The semantic parent may still be uninteresting to us,
   * so possibly keep walking up the AST until we get to
   * something better.
   */
  do
    {
      cursor = clang_getCursorSemanticParent (cursor);
      kind = clang_getCursorKind (cursor);
    }
  while (!clang_Cursor_isNull (cursor) && is_ignored_kind (kind));

  if (kind == CXCursor_TranslationUnit)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "The location does not have a semantic parent");
      IDE_RETURN (NULL);
    }

  symbol_location = ide_source_location_new (ifile, line - 1, line_offset - 1, 0);
  cxname = clang_getCursorSpelling (cursor);
  symkind = get_symbol_kind (cursor, &symflags);

  ret = ide_symbol_new (clang_getCString (cxname),
                        symkind,
                        symflags,
                        NULL,
                        NULL,
                        symbol_location);

  IDE_TRACE_MSG ("Symbol = %p", ret);

  IDE_RETURN (ret);
}

gchar *
ide_clang_translation_unit_generate_key (IdeClangTranslationUnit  *self,
                                         IdeSourceLocation        *location)
{
  g_auto(CXString) cx_usr = {0};
  CXTranslationUnit unit;
  CXFile file;
  CXSourceLocation cx_location;
  CXCursor reference;
  CXCursor declaration;
  const gchar *usr;
  guint line = 0;
  guint column = 0;
  enum CXLinkageKind linkage;

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);

  unit = ide_ref_ptr_get (self->native);

  file = get_file_for_location (self, location);
  line = ide_source_location_get_line (location);
  column = ide_source_location_get_line_offset (location);

  cx_location = clang_getLocation (unit, file, line + 1, column + 1);

  reference = clang_getCursor (unit, cx_location);
  declaration = clang_getCursorReferenced (reference);
  cx_usr = clang_getCursorUSR (declaration);
  usr = clang_getCString (cx_usr);
  linkage = clang_getCursorLinkage (declaration);

  if (linkage == CXLinkage_Internal || linkage == CXLinkage_NoLinkage || usr == NULL)
    return NULL;

  return g_strdup (usr);
}
