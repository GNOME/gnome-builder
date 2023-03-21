/* ide-project.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-project"

#include "config.h"

#include <glib/gi18n.h>

#include <libpeas.h>

#include "ide-marshal.h"

#include <libide-code.h>
#include <libide-plugins.h>

#include "ide-buffer-private.h"

#include "ide-project.h"
#include "ide-similar-file-locator.h"

struct _IdeProject
{
  IdeObject               parent_instance;
  IdeExtensionSetAdapter *similar_file_locators;
};

typedef struct
{
  GFile     *orig_file;
  GFile     *new_file;
  IdeBuffer *buffer;
} RenameFile;

enum {
  FILE_RENAMED,
  FILE_TRASHED,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (IdeProject, ide_project, IDE_TYPE_OBJECT)

static guint signals [N_SIGNALS];

static void
ide_project_destroy (IdeObject *object)
{
  IdeProject *self = (IdeProject *)object;

  ide_clear_and_destroy_object (&self->similar_file_locators);

  IDE_OBJECT_CLASS (ide_project_parent_class)->destroy (object);
}

static void
ide_project_class_init (IdeProjectClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_project_destroy;

  signals [FILE_RENAMED] =
    g_signal_new ("file-renamed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_FILE | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_FILE | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [FILE_RENAMED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECT_OBJECTv);

  signals [FILE_TRASHED] =
    g_signal_new ("file-trashed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_FILE | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [FILE_TRASHED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);
}

static void
ide_project_init (IdeProject *self)
{
}

/**
 * ide_project_from_context:
 * @context: #IdeContext
 *
 * Gets the project for an #IdeContext.
 *
 * Returns: (transfer none): an #IdeProject
 */
IdeProject *
ide_project_from_context (IdeContext *context)
{
  IdeProject *self;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  /* Return borrowed reference */
  self = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_PROJECT);
  g_object_unref (self);

  return self;
}

static void
rename_file_free (gpointer data)
{
  RenameFile *op = data;

  g_assert (IDE_IS_MAIN_THREAD ());

  if (op != NULL)
    {
      g_clear_object (&op->new_file);
      g_clear_object (&op->orig_file);
      g_clear_object (&op->buffer);
      g_slice_free (RenameFile, op);
    }
}

static gboolean
emit_file_renamed (gpointer data)
{
  IdeTask *task = data;
  IdeProject *project;
  RenameFile *rf;

  g_assert (IDE_IS_TASK (task));

  project = ide_task_get_source_object (task);
  rf = ide_task_get_task_data (task);

  g_assert (IDE_IS_PROJECT (project));
  g_assert (rf != NULL);
  g_assert (G_IS_FILE (rf->orig_file));
  g_assert (G_IS_FILE (rf->new_file));

  g_signal_emit (project,
                 signals [FILE_RENAMED],
                 0,
                 rf->orig_file,
                 rf->new_file);

  return G_SOURCE_REMOVE;
}

static void
ide_project_rename_file_worker (IdeTask      *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  IdeProject *self = source_object;
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GError) error = NULL;
  RenameFile *op = task_data;
  IdeContext *context;

  g_assert (IDE_IS_PROJECT (self));
  g_assert (op != NULL);
  g_assert (G_IS_FILE (op->orig_file));
  g_assert (G_IS_FILE (op->new_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);
  path = g_file_get_relative_path (workdir, op->new_file);

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *old_path = g_file_get_uri (op->orig_file);
    g_autofree gchar *new_path = g_file_get_uri (op->new_file);
    IDE_TRACE_MSG ("Renaming %s to %s", old_path, new_path);
  }
#endif

  if (path == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 _("Destination file must be within the project tree."));
      return;
    }

  parent = g_file_get_parent (op->new_file);

  if (!g_file_query_exists (parent, cancellable) &&
      !g_file_make_directory_with_parents (parent, cancellable, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!g_file_move (op->orig_file,
                    op->new_file,
                    G_FILE_COPY_NONE,
                    cancellable,
                    NULL,
                    NULL,
                    &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_idle_add_full (G_PRIORITY_LOW,
                   emit_file_renamed,
                   g_object_ref (task),
                   g_object_unref);

  ide_task_return_boolean (task, TRUE);
}

static void
ide_project_rename_buffer_save_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  RenameFile *rf;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_buffer_save_file_finish (buffer, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  rf = ide_task_get_task_data (task);
  g_assert (rf != NULL);
  g_assert (G_IS_FILE (rf->orig_file));
  g_assert (G_IS_FILE (rf->new_file));
  g_assert (IDE_IS_BUFFER (rf->buffer));

  /*
   * Change the filename in the buffer so that the user doesn't continue
   * to edit the file under the old name.
   */
  _ide_buffer_set_file (rf->buffer, rf->new_file);

  ide_task_run_in_thread (task, ide_project_rename_file_worker);
}

void
ide_project_rename_file_async (IdeProject          *self,
                               GFile               *orig_file,
                               GFile               *new_file,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeBufferManager *bufmgr;
  IdeContext *context;
  IdeBuffer *buffer;
  RenameFile *op;

  g_return_if_fail (IDE_IS_PROJECT (self));
  g_return_if_fail (G_IS_FILE (orig_file));
  g_return_if_fail (G_IS_FILE (new_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_project_rename_file_async);
  ide_task_set_release_on_propagate (task, FALSE);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  bufmgr = ide_buffer_manager_from_context (context);
  buffer = ide_buffer_manager_find_buffer (bufmgr, orig_file);

  op = g_slice_new0 (RenameFile);
  op->orig_file = g_object_ref (orig_file);
  op->new_file = g_object_ref (new_file);
  op->buffer = buffer ? g_object_ref (buffer) : NULL;
  ide_task_set_task_data (task, op, rename_file_free);

  /*
   * If the file is open and has any changes, we need to save those
   * changes before we can proceed.
   */
  if (buffer != NULL)
    {
      if (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)))
        {
          ide_buffer_save_file_async (buffer,
                                      orig_file,
                                      NULL,
                                      NULL,
                                      ide_project_rename_buffer_save_cb,
                                      g_steal_pointer (&task));
          return;
        }

      _ide_buffer_set_file (buffer, new_file);
    }

  ide_task_run_in_thread (task, ide_project_rename_file_worker);
}

gboolean
ide_project_rename_file_finish (IdeProject    *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_PROJECT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (task), FALSE);

  return ide_task_propagate_boolean (task, error);
}

static void
ide_project_trash_file__file_trash_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeProject *self;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_file_trash_finish (file, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  self = ide_task_get_source_object (task);

  g_assert (IDE_IS_PROJECT (self));

  g_signal_emit (self, signals [FILE_TRASHED], 0, file);
}

static void
ide_project_trash_file__wait_check_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeProject *self;
  GFile *file;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  self = ide_task_get_source_object (task);
  file = ide_task_get_task_data (task);

  g_assert (IDE_IS_PROJECT (self));
  g_assert (G_IS_FILE (file));

  g_signal_emit (self, signals [FILE_TRASHED], 0, file);
}

static gboolean
file_is_ancestor (GFile *file,
                  GFile *maybe_child)
{
  gchar *path;
  gboolean ret;

  path = g_file_get_relative_path (file, maybe_child);
  ret = (path != NULL);
  g_free (path);

  return ret;
}

void
ide_project_trash_file_async (IdeProject          *self,
                              GFile               *file,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_PROJECT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_project_trash_file_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

  if (!file_is_ancestor (workdir, file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 _("File must be within the project tree."));
      IDE_EXIT;
    }

  if (ide_is_flatpak ())
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autoptr(IdeSubprocess) subprocess = NULL;
      g_autoptr(GError) error = NULL;
      g_autofree gchar *uri = g_file_get_uri (file);

      /*
       * FIXME: Use proper GIO trashing under Flatpak
       *
       * This manually trashes the file by running the "gio trash" command
       * on the host system. This is a pretty bad way to do this, but it is
       * required until GIO works properly under Flatpak for detecting files
       * crossing bind mounts.
       *
       * https://bugzilla.gnome.org/show_bug.cgi?id=780472
       */

      launcher = ide_subprocess_launcher_new (0);
      ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
      ide_subprocess_launcher_push_argv (launcher, "gio");
      ide_subprocess_launcher_push_argv (launcher, "trash");
      ide_subprocess_launcher_push_argv (launcher, uri);

      subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

      if (subprocess == NULL)
        ide_task_return_error (task, g_steal_pointer (&error));
      else
        ide_subprocess_wait_check_async (subprocess,
                                         cancellable,
                                         ide_project_trash_file__wait_check_cb,
                                         g_steal_pointer (&task));

      IDE_EXIT;
    }
  else
    {
      g_file_trash_async (file,
                          G_PRIORITY_DEFAULT,
                          cancellable,
                          ide_project_trash_file__file_trash_cb,
                          g_steal_pointer (&task));

      IDE_EXIT;
    }
}

gboolean
ide_project_trash_file_finish (IdeProject    *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  IdeTask *task = (IdeTask *)result;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_PROJECT (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  ret = ide_task_propagate_boolean (task, error);

  IDE_RETURN (ret);
}

typedef struct
{
  GFile *file;
  GListStore *models;
  guint n_active;
} ListSimilar;

static void
list_similar_free (ListSimilar *state)
{
  g_assert (state->n_active == 0);

  g_clear_object (&state->file);
  g_clear_object (&state->models);
  g_slice_free (ListSimilar, state);
}

static void
ide_project_list_similar_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeSimilarFileLocator *locator = (IdeSimilarFileLocator *)object;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  ListSimilar *state;
  IdeProject *self;

  g_assert (IDE_IS_SIMILAR_FILE_LOCATOR (locator));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  g_assert (IDE_IS_PROJECT (self));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));
  g_assert (G_IS_LIST_STORE (state->models));

  if ((model = ide_similar_file_locator_list_finish (locator, result, &error)))
    g_list_store_append (state->models, model);
  else if (!ide_error_ignore (error))
    ide_object_warning (IDE_OBJECT (self), "%s", error->message);

  state->n_active--;

  if (state->n_active == 0)
    ide_task_return_object (task,
                            gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (state->models))));
}

static void
ide_project_list_similar_foreach_cb (IdeExtensionSetAdapter *set,
                                     PeasPluginInfo         *plugin_info,
                                     GObject          *exten,
                                     gpointer                user_data)
{
  IdeSimilarFileLocator *locator = (IdeSimilarFileLocator *)exten;
  IdeTask *task = user_data;
  ListSimilar *state;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SIMILAR_FILE_LOCATOR (locator));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->file));
  g_assert (G_IS_LIST_STORE (state->models));

  state->n_active++;

  ide_similar_file_locator_list_async (locator,
                                       state->file,
                                       ide_task_get_cancellable (task),
                                       ide_project_list_similar_cb,
                                       g_object_ref (task));
}

void
ide_project_list_similar_async (IdeProject          *self,
                                GFile               *file,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  ListSimilar *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_PROJECT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_project_list_similar_async);

  if (self->similar_file_locators == NULL)
    self->similar_file_locators = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                                 peas_engine_get_default (),
                                                                 IDE_TYPE_SIMILAR_FILE_LOCATOR,
                                                                 NULL, NULL);

  state = g_slice_new0 (ListSimilar);
  state->file = g_object_ref (file);
  state->models = g_list_store_new (G_TYPE_LIST_MODEL);
  ide_task_set_task_data (task, state, list_similar_free);

  ide_extension_set_adapter_foreach (self->similar_file_locators,
                                     ide_project_list_similar_foreach_cb,
                                     task);

  if (state->n_active == 0)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Not supported");

  IDE_EXIT;
}

/**
 * ide_project_list_similar_finish:
 * @self: a #IdeProject
 * @result: a #GAsyncResult
 * @error: location for a #GError
 *
 * Completes asynchronous request to locate similar files.
 *
 * Returns: (transfer full): a #GListModel of #GFile or %NULL
 */
GListModel *
ide_project_list_similar_finish (IdeProject    *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_PROJECT (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (ret);
}
