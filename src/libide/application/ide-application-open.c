/* ide-application-open.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-application-open"

#include "application/ide-application.h"
#include "application/ide-application-private.h"
#include "workbench/ide-workbench.h"
#include "vcs/ide-vcs.h"
#include "threading/ide-task.h"

typedef struct
{
  GPtrArray *files;
  gchar     *hint;
} IdeApplicationOpen;

static void ide_application_open_tick (IdeTask *task);

static void
ide_application_open_free (gpointer data)
{
  IdeApplicationOpen *state = data;

  g_free (state->hint);
  g_ptr_array_unref (state->files);
  g_slice_free (IdeApplicationOpen, state);
}

static gboolean
workbench_manages_file (IdeWorkbench *workbench,
                        GFile        *file)
{
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (G_IS_FILE (file));

  if (NULL == (context = ide_workbench_get_context (workbench)))
    return FALSE;

  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  return g_file_has_prefix (file, workdir);
}

static gboolean
maybe_open_with_existing_workspace (IdeApplication *self,
                                    GFile          *file,
                                    const gchar    *hint,
                                    GCancellable   *cancellable)
{
  GList *windows;
  GList *iter;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (G_IS_FILE (file));

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (iter = windows; iter != NULL; iter = iter->next)
    {
      GtkWindow *window = iter->data;

      if (IDE_IS_WORKBENCH (window) &&
          workbench_manages_file (IDE_WORKBENCH (window), file))
        {
          ide_workbench_open_files_async (IDE_WORKBENCH (window),
                                          &file,
                                          1,
                                          hint,
                                          0,
                                          cancellable,
                                          NULL,
                                          NULL);
          return TRUE;
        }
    }

  return FALSE;
}

static void
ide_application_open_project_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeWorkbench *workbench = (IdeWorkbench *)object;
  IdeApplicationOpen *state;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  file = g_object_ref (g_ptr_array_index (state->files, state->files->len - 1));
  g_ptr_array_remove_index (state->files, state->files->len - 1);

  if (!ide_workbench_open_project_finish (workbench, result, &error))
    {
      g_warning ("%s", error->message);
      gtk_widget_destroy (GTK_WIDGET (workbench));
    }
  else
    {
      ide_workbench_open_files_async (workbench,
                                      &file, 1,
                                      state->hint,
                                      0,
                                      ide_task_get_cancellable (task),
                                      NULL,
                                      NULL);
      gtk_window_present (GTK_WINDOW (workbench));
    }

  ide_application_open_tick (task);
}

static void
ide_application_open_tick (IdeTask *task)
{
  IdeApplication *self;
  IdeApplicationOpen *state;
  IdeWorkbench *workbench;
  GCancellable *cancellable;
  GFile *next;
  guint i;

  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (state != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * Try to open each of our available files with an existing workspace
   * since we could have gotten a new workspace since the last file
   * we opened.
   */

  for (i = state->files->len; i > 0; i--)
    {
      GFile *file = g_ptr_array_index (state->files, i - 1);

      /*
       * We walk backwards via the array so we can safely remove
       * items as we go. We could do remove_index_fast(), but it
       * seems worthwhile to preserve the stack ordering as much
       * as possible. This way, the files are shown in the editor
       * with a similar stacking to the request.
       */
      if (maybe_open_with_existing_workspace (self, file, state->hint, cancellable))
        g_ptr_array_remove_index (state->files, i - 1);
    }

  /*
   * If we have no files left, we can complete the task now.
   */
  if (state->files->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  /*
   * Try to open the next file in the list, which will result in a
   * new workbench being loaded (and therefore might allow us to
   * open further files in that workbench).
   */

  next = g_ptr_array_index (state->files, state->files->len - 1);

  workbench = g_object_new (IDE_TYPE_WORKBENCH,
                            "application", self,
                            "disable-greeter", TRUE,
                            NULL);

  ide_workbench_open_project_async (workbench,
                                    next,
                                    cancellable,
                                    ide_application_open_project_cb,
                                    g_object_ref (task));
}

void
ide_application_open_async (IdeApplication       *self,
                            GFile               **files,
                            gint                  n_files,
                            const gchar          *hint,
                            GCancellable         *cancellable,
                            GAsyncReadyCallback   callback,
                            gpointer              user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  IdeApplicationOpen *state;
  guint i;

  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (!n_files || files != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_application_open_async);
  ide_task_set_check_cancellable (task, FALSE);

  /*
   * We have to open each file one at a time so that we don't race to
   * open the same containing project multiple times.
   */

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  for (i = 0; i < n_files; i++)
    {
      GFile *file = files [i];

      if (!maybe_open_with_existing_workspace (self, file, hint, cancellable))
        g_ptr_array_add (ar, g_object_ref (file));
    }

  state = g_slice_new0 (IdeApplicationOpen);
  state->hint = g_strdup (hint);
  state->files = g_steal_pointer (&ar);

  ide_task_set_task_data (task, state, ide_application_open_free);
  ide_application_open_tick (task);
}

gboolean
ide_application_open_finish (IdeApplication  *self,
                             GAsyncResult    *result,
                             GError         **error)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
