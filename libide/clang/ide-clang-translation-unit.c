/* ide-clang-translation-unit.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
#include <glib/gi18n.h>

#include "egg-counter.h"

#include "ide-context.h"
#include "ide-clang-completion-item.h"
#include "ide-clang-private.h"
#include "ide-clang-symbol-tree.h"
#include "ide-clang-translation-unit.h"
#include "ide-debug.h"
#include "ide-diagnostic.h"
#include "ide-diagnostics.h"
#include "ide-file.h"
#include "ide-internal.h"
#include "ide-project.h"
#include "ide-ref-ptr.h"
#include "ide-source-location.h"
#include "ide-symbol.h"
#include "ide-thread-pool.h"
#include "ide-unsaved-file.h"
#include "ide-unsaved-files.h"
#include "ide-vcs.h"

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
EGG_DEFINE_COUNTER (instances, "Clang", "Translation Units", "Number of clang translation units")

enum {
  PROP_0,
  PROP_FILE,
  PROP_INDEX,
  PROP_NATIVE,
  PROP_SERIAL,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

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

/**
 * ide_clang_translation_unit_get_index:
 * @self: A #IdeClangTranslationUnit.
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
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);
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
  if (g_str_has_prefix (path, workpath))
    {
      path = path + strlen (workpath);
      while (*path == G_DIR_SEPARATOR)
        path++;

      return g_strdup (path);
    }

  return g_strdup (path);
}

static IdeSourceLocation *
create_location (IdeClangTranslationUnit *self,
                 IdeProject              *project,
                 const gchar             *workpath,
                 CXSourceLocation         cxloc)
{
  IdeSourceLocation *ret = NULL;
  IdeFile *file = NULL;
  CXFile cxfile = NULL;
  g_autofree gchar *path = NULL;
  const gchar *cstr;
  CXString str;
  unsigned line;
  unsigned column;
  unsigned offset;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (workpath, NULL);

  clang_getFileLocation (cxloc, &cxfile, &line, &column, &offset);

  if (line > 0) line--;
  if (column > 0) column--;

  str = clang_getFileName (cxfile);
  cstr = clang_getCString (str);
  if (cstr != NULL)
    path = get_path (workpath, cstr);
  clang_disposeString (str);
  if (cstr == NULL)
    return NULL;

  file = ide_project_get_file_for_path (project, path);

  if (!file)
    {
      IdeContext *context;
      GFile *gfile;

      context = ide_object_get_context (IDE_OBJECT (self));
      gfile = g_file_new_for_path (path);

      file = g_object_new (IDE_TYPE_FILE,
                           "context", context,
                           "file", gfile,
                           "path", path,
                           NULL);
    }

  ret = ide_source_location_new (file, line, column, offset);

  return ret;
}

static IdeSourceRange *
create_range (IdeClangTranslationUnit *self,
              IdeProject              *project,
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

  begin = create_location (self, project, workpath, cxbegin);
  end = create_location (self, project, workpath, cxend);

  if ((begin != NULL) && (end != NULL))
    range = _ide_source_range_new (begin, end);

  return range;
}

static gboolean
cxfile_equal (CXFile  cxfile,
              GFile  *file)
{
  CXString cxstr;
  gchar *path;
  gboolean ret;

  cxstr = clang_getFileName (cxfile);
  path = g_file_get_path (file);

  ret = (0 == g_strcmp0 (clang_getCString (cxstr), path));

  clang_disposeString (cxstr);
  g_free (path);

  return ret;
}

static IdeDiagnostic *
create_diagnostic (IdeClangTranslationUnit *self,
                   IdeProject              *project,
                   const gchar             *workpath,
                   GFile                   *target,
                   CXDiagnostic            *cxdiag)
{
  enum CXDiagnosticSeverity cxseverity;
  IdeDiagnosticSeverity severity;
  IdeDiagnostic *diag;
  IdeSourceLocation *loc;
  g_autofree gchar *spelling = NULL;
  CXString cxstr;
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
  spelling = g_strdup (clang_getCString (cxstr));
  clang_disposeString (cxstr);

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

  loc = create_location (self, project, workpath, cxloc);

  diag = _ide_diagnostic_new (severity, spelling, loc);

  num_ranges = clang_getDiagnosticNumRanges (cxdiag);

  for (i = 0; i < num_ranges; i++)
    {
      CXSourceRange cxrange;
      IdeSourceRange *range;

      cxrange = clang_getDiagnosticRange (cxdiag, i);
      range = create_range (self, project, workpath, cxrange);
      if (range != NULL)
        _ide_diagnostic_take_range (diag, range);
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
      IdeContext *context;
      IdeProject *project;
      IdeVcs *vcs;
      g_autofree gchar *workpath = NULL;
      GFile *workdir;
      GPtrArray *diags;
      guint count;
      guint i;

      diags = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_diagnostic_unref);

      /*
       * Acquire the reader lock for the project since we will need to do
       * a bunch of project tree lookups when creating diagnostics. By doing
       * this outside of the loops, we avoid creating lots of contention on
       * the reader lock, but potentially hold on to the entire lock for a bit
       * longer at a time.
       */
      context = ide_object_get_context (IDE_OBJECT (self));
      project = ide_context_get_project (context);
      vcs = ide_context_get_vcs (context);
      workdir = ide_vcs_get_working_directory (vcs);
      workpath = g_file_get_path (workdir);

      ide_project_reader_lock (project);

      count = clang_getNumDiagnostics (tu);
      for (i = 0; i < count; i++)
        {
          CXDiagnostic cxdiag;
          IdeDiagnostic *diag;

          cxdiag = clang_getDiagnostic (tu, i);
          diag = create_diagnostic (self, project, workpath, file, cxdiag);

          if (diag != NULL)
            {
              guint num_fixits;
              gsize j;

              num_fixits = clang_getDiagnosticNumFixIts (cxdiag);

              for (j = 0; j < num_fixits; j++)
                {
                  IdeFixit *fixit = NULL;
                  IdeSourceRange *range;
                  CXSourceRange cxrange;
                  CXString cxstr;

                  cxstr = clang_getDiagnosticFixIt (cxdiag, j, &cxrange);
                  range = create_range (self, project, workpath, cxrange);
                  fixit = _ide_fixit_new (range, clang_getCString (cxstr));
                  clang_disposeString (cxstr);

                  if (fixit != NULL)
                    _ide_diagnostic_take_fixit (diag, fixit);
                }

              g_ptr_array_add (diags, diag);
            }

          clang_disposeDiagnostic (cxdiag);
        }

      ide_project_reader_unlock (project);

      g_hash_table_insert (self->diagnostics, g_object_ref (file), _ide_diagnostics_new (diags));
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

  EGG_COUNTER_DEC (instances);

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

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The file used to build the translation unit."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_INDEX] =
    g_param_spec_boxed ("index",
                         _("Index"),
                         _("The highlight index for the translation unit."),
                         IDE_TYPE_HIGHLIGHT_INDEX,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_NATIVE] =
    g_param_spec_pointer ("native",
                          _("Native"),
                          _("The native translation unit pointer."),
                          (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_SERIAL] =
    g_param_spec_int64 ("serial",
                        _("Serial"),
                        _("A sequence number for the translation unit."),
                        0,
                        G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
ide_clang_translation_unit_init (IdeClangTranslationUnit *self)
{
  EGG_COUNTER_INC (instances);

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
  g_autoptr(GPtrArray) ar = NULL;
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
  ar = g_ptr_array_new ();

  for (i = 0; i < results->NumResults; i++)
    {
      GtkSourceCompletionProposal *proposal;

      proposal = g_object_new (IDE_TYPE_CLANG_COMPLETION_ITEM,
                               "results", ide_ref_ptr_ref (refptr),
                               "index", (guint)i,
                               NULL);
      g_ptr_array_add (ar, proposal);
    }

  g_task_return_pointer (task, g_ptr_array_ref (ar), (GDestroyNotify)g_ptr_array_unref);

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
 * @self: A #IdeClangTranslationUnit.
 * @result: A #GAsyncResult
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
  g_autofree gchar *filename = NULL;
  g_autofree gchar *workpath = NULL;
  g_auto(CXString) cxstr = { 0 };
  g_autoptr(IdeSourceLocation) declaration = NULL;
  g_autoptr(IdeSourceLocation) definition = NULL;
  g_autoptr(IdeSourceLocation) canonical = NULL;
  CXTranslationUnit tu;
  IdeSymbolKind symkind = 0;
  IdeSymbolFlags symflags = 0;
  IdeProject *project;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  CXSourceLocation cxlocation;
  CXCursor tmpcursor;
  CXCursor cursor;
  CXFile cxfile;
  IdeSymbol *ret = NULL;
  IdeFile *file;
  GFile *gfile;
  guint line;
  guint line_offset;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);
  g_return_val_if_fail (location != NULL, NULL);

  tu = ide_ref_ptr_get (self->native);

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  workpath = g_file_get_path (workdir);

  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  if (!(file = ide_source_location_get_file (location)) ||
      !(gfile = ide_file_get_file (file)) ||
      !(filename = g_file_get_path (gfile)) ||
      !(cxfile = clang_getFile (tu, filename)))
    IDE_RETURN (NULL);

  cxlocation = clang_getLocation (tu, cxfile, line + 1, line_offset + 1);
  cursor = clang_getCursor (tu, cxlocation);
  if (clang_Cursor_isNull (cursor))
    IDE_RETURN (NULL);

  tmpcursor = clang_getCursorReferenced (cursor);
  if (!clang_Cursor_isNull (tmpcursor))
    {
      CXSourceLocation tmploc;
      CXSourceRange cxrange;

      cxrange = clang_getCursorExtent (tmpcursor);
      tmploc = clang_getRangeStart (cxrange);
      definition = create_location (self, project, workpath, tmploc);
    }

  symkind = get_symbol_kind (cursor, &symflags);

  cxstr = clang_getCursorDisplayName (cursor);
  ret = _ide_symbol_new (clang_getCString (cxstr), symkind, symflags,
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
  g_auto(CXString) cxname = { 0 };
  g_autoptr(IdeSourceLocation) srcloc = NULL;
  CXSourceLocation cxloc;
  IdeSymbolKind symkind;
  IdeSymbolFlags symflags;
  const gchar *name;
  IdeSymbol *symbol;
  guint line;
  guint line_offset;

  cxname = clang_getCursorSpelling (cursor);
  name = clang_getCString (cxname);
  cxloc = clang_getCursorLocation (cursor);
  clang_getFileLocation (cxloc, NULL, &line, &line_offset, NULL);
  srcloc = ide_source_location_new (state->file, line-1, line_offset-1, 0);

  symkind = get_symbol_kind (cursor, &symflags);

  symbol = _ide_symbol_new (name, symkind, symflags, NULL, NULL, srcloc);

  return symbol;
}

static enum CXChildVisitResult
ide_clang_translation_unit_get_symbols__visitor_cb (CXCursor     cursor,
                                                    CXCursor     parent,
                                                    CXClientData user_data)
{
  GetSymbolsState *state = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_auto(CXString) filename = { 0 };
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
 * Returns: (transfer container) (element-type IdeSymbol*): An array of #IdeSymbol.
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
  IdeSymbolTree *symbol_tree;
  IdeContext *context;

  g_return_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  context = ide_object_get_context (IDE_OBJECT (self));
  symbol_tree = g_object_new (IDE_TYPE_CLANG_SYMBOL_TREE,
                              "context", context,
                              "native", self->native,
                              "file", file,
                              NULL);
  g_task_return_pointer (task, symbol_tree, g_object_unref);
}

IdeSymbolTree *
ide_clang_translation_unit_get_symbol_tree_finish (IdeClangTranslationUnit  *self,
                                                   GAsyncResult             *result,
                                                   GError                  **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}
