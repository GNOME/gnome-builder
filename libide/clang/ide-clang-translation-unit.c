/* ide-clang-translation-unit.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "clang-translation-unit"

#include <clang-c/Index.h>
#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-clang-completion-item.h"
#include "ide-clang-private.h"
#include "ide-clang-translation-unit.h"
#include "ide-debug.h"
#include "ide-diagnostic.h"
#include "ide-diagnostics.h"
#include "ide-file.h"
#include "ide-internal.h"
#include "ide-project.h"
#include "ide-ref-ptr.h"
#include "ide-source-location.h"
#include "ide-unsaved-file.h"
#include "ide-unsaved-files.h"
#include "ide-vcs.h"

struct _IdeClangTranslationUnit
{
  IdeObject          parent_instance;

  CXTranslationUnit  tu;
  gint64             sequence;
  IdeDiagnostics    *diagnostics;
  GFile             *file;
  IdeHighlightIndex *index;
};

typedef struct
{
  GPtrArray *unsaved_files;
  gchar     *path;
  guint      line;
  guint      line_offset;
} CodeCompleteState;

G_DEFINE_TYPE (IdeClangTranslationUnit, ide_clang_translation_unit, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_FILE,
  PROP_INDEX,
  PROP_SEQUENCE,
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
                                 gint64             sequence)
{
  IdeClangTranslationUnit *ret;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (tu, NULL);
  g_return_val_if_fail (!file || G_IS_FILE (file), NULL);

  ret = g_object_new (IDE_TYPE_CLANG_TRANSLATION_UNIT,
                      "context", context,
                      "file", file,
                      "index", index,
                      NULL);

  ret->tu = tu;
  ret->sequence = sequence;

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
  if (!cstr)
    return NULL;

  path = get_path (workpath, cstr);
  clang_disposeString (str);

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

  if (cxfile && !cxfile_equal (cxfile, self->file))
    return NULL;

  cxseverity = clang_getDiagnosticSeverity (cxdiag);
  severity = translate_severity (cxseverity);

  cxstr = clang_getDiagnosticSpelling (cxdiag);
  spelling = g_strdup (clang_getCString (cxstr));
  clang_disposeString (cxstr);

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
 * ide_clang_translation_unit_get_diagnostics:
 *
 * Retrieves the diagnostics for the translation unit.
 *
 * Returns: (transfer none) (nullable): An #IdeDiagnostics or %NULL.
 */
IdeDiagnostics *
ide_clang_translation_unit_get_diagnostics (IdeClangTranslationUnit *self)
{

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);

  if (!self->diagnostics)
    {
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

      count = clang_getNumDiagnostics (self->tu);
      for (i = 0; i < count; i++)
        {
          CXDiagnostic cxdiag;
          IdeDiagnostic *diag;

          cxdiag = clang_getDiagnostic (self->tu, i);
          diag = create_diagnostic (self, project, workpath, cxdiag);

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

      self->diagnostics = _ide_diagnostics_new (diags);
    }

  return self->diagnostics;
}

gint64
ide_clang_translation_unit_get_sequence (IdeClangTranslationUnit *self)
{
  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), -1);

  return self->sequence;
}

static void
ide_clang_translation_unit_finalize (GObject *object)
{
  IdeClangTranslationUnit *self = (IdeClangTranslationUnit *)object;

  IDE_ENTRY;

  clang_disposeTranslationUnit (self->tu);
  g_clear_pointer (&self->diagnostics, ide_diagnostics_unref);
  g_clear_object (&self->file);
  g_clear_pointer (&self->index, ide_highlight_index_unref);

  G_OBJECT_CLASS (ide_clang_translation_unit_parent_class)->finalize (object);

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

    case PROP_SEQUENCE:
      g_value_set_int64 (value, ide_clang_translation_unit_get_sequence (self));
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
  g_object_class_install_property (object_class, PROP_FILE, gParamSpecs [PROP_FILE]);

  gParamSpecs [PROP_INDEX] =
    g_param_spec_boxed ("index",
                         _("Index"),
                         _("The highlight index for the translation unit."),
                         IDE_TYPE_HIGHLIGHT_INDEX,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INDEX, gParamSpecs [PROP_INDEX]);

  gParamSpecs [PROP_SEQUENCE] =
    g_param_spec_int64 ("sequence",
                        _("Sequence"),
                        _("The sequence number when created."),
                        G_MININT64,
                        G_MAXINT64,
                        0,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SEQUENCE, gParamSpecs [PROP_SEQUENCE]);
}

static void
ide_clang_translation_unit_init (IdeClangTranslationUnit *self)
{
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
  g_autoptr(IdeRefPtr) refptr = NULL;
  struct CXUnsavedFile *ufs;
  g_autoptr(GPtrArray) ar = NULL;
  gsize i;
  gsize j = 0;

  g_assert (IDE_IS_CLANG_TRANSLATION_UNIT (self));
  g_assert (state);
  g_assert (state->unsaved_files);

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

  results = clang_codeCompleteAt (self->tu,
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
  g_autofree gchar *path = NULL;
  CodeCompleteState *state;
  IdeContext *context;
  IdeUnsavedFiles *unsaved_files;

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
  state->unsaved_files = ide_unsaved_files_get_unsaved_files (unsaved_files);

  /*
   * TODO: Technically it is not safe for us to go run this in a thread. We need to ensure
   *       that only one thread is dealing with this at a time.
   */

  g_task_set_task_data (task, state, code_complete_state_free);
  g_task_run_in_thread (task, ide_clang_translation_unit_code_complete_worker);
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

  g_return_val_if_fail (IDE_IS_CLANG_TRANSLATION_UNIT (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}
