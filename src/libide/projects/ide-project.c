/* ide-project.c
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

#define G_LOG_DOMAIN "ide-project"

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "application/ide-application.h"
#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-manager.h"
#include "files/ide-file.h"
#include "projects/ide-project-item.h"
#include "projects/ide-project.h"
#include "subprocess/ide-subprocess.h"
#include "subprocess/ide-subprocess-launcher.h"
#include "util/ide-flatpak.h"
#include "vcs/ide-vcs.h"

struct _IdeProject
{
  IdeObject       parent_instance;

  GRWLock         rw_lock;
  IdeProjectItem *root;
  gchar          *name;
  gchar          *id;
};

typedef struct
{
  GFile     *orig_file;
  GFile     *new_file;
  IdeBuffer *buffer;
} RenameFile;

G_DEFINE_TYPE (IdeProject, ide_project, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_ROOT,
  LAST_PROP
};

enum {
  FILE_RENAMED,
  FILE_TRASHED,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

void
ide_project_reader_lock (IdeProject *self)
{
  g_return_if_fail (IDE_IS_PROJECT (self));

  g_rw_lock_reader_lock (&self->rw_lock);
}

void
ide_project_reader_unlock (IdeProject *self)
{
  g_return_if_fail (IDE_IS_PROJECT (self));

  g_rw_lock_reader_unlock (&self->rw_lock);
}

void
ide_project_writer_lock (IdeProject *self)
{
  g_return_if_fail (IDE_IS_PROJECT (self));

  g_rw_lock_writer_lock (&self->rw_lock);
}

void
ide_project_writer_unlock (IdeProject *self)
{
  g_return_if_fail (IDE_IS_PROJECT (self));

  g_rw_lock_writer_unlock (&self->rw_lock);
}

/**
 * ide_project_create_id:
 * @name: the name of the project
 *
 * Escapes the project name into something suitable using as an id.
 * This can be uesd to determine the directory name when the project
 * name should be used.
 *
 * Returns: (transfer full): a new string
 *
 * Since: 3.28
 */
gchar *
ide_project_create_id (const gchar *name)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_strdelimit (g_strdup (name), " /|<>\n\t", '-');
}

const gchar *
ide_project_get_id (IdeProject *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT (self), NULL);

  return self->id;
}

const gchar *
ide_project_get_name (IdeProject *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT (self), NULL);

  return self->name;
}

void
_ide_project_set_name (IdeProject  *self,
                       const gchar *name)
{
  g_return_if_fail (IDE_IS_PROJECT (self));

  if (self->name != name)
    {
      g_free (self->name);
      self->name = g_strdup (name);
      self->id = ide_project_create_id (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

/**
 * ide_project_get_root:
 *
 * Retrieves the root item of the project tree.
 *
 * You must be holding the reader lock while calling and using the result of
 * this function. Other thread may be accessing or modifying the tree without
 * your knowledge. See ide_project_reader_lock() and ide_project_reader_unlock()
 * for more information.
 *
 * If you need to modify the tree, you must hold a writer lock that has been
 * acquired with ide_project_writer_lock() and released with
 * ide_project_writer_unlock() when you are no longer modifiying the tree.
 *
 * Returns: (transfer none): An #IdeProjectItem.
 */
IdeProjectItem *
ide_project_get_root (IdeProject *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT (self),  NULL);

  return self->root;
}

static void
ide_project_set_root (IdeProject     *self,
                      IdeProjectItem *root)
{
  g_autoptr(IdeProjectItem) allocated = NULL;
  IdeContext *context;

  g_return_if_fail (IDE_IS_PROJECT (self));
  g_return_if_fail (!root || IDE_IS_PROJECT_ITEM (root));

  context = ide_object_get_context (IDE_OBJECT (self));

  if (!root)
    {
      allocated = g_object_new (IDE_TYPE_PROJECT_ITEM,
                                "context", context,
                                NULL);
      root = allocated;
    }

  if (g_set_object (&self->root, root))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ROOT]);
}

static void
ide_project_finalize (GObject *object)
{
  IdeProject *self = (IdeProject *)object;

  g_clear_object (&self->root);
  g_clear_pointer (&self->name, g_free);
  g_rw_lock_clear (&self->rw_lock);

  G_OBJECT_CLASS (ide_project_parent_class)->finalize (object);
}

static void
ide_project_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeProject *self = IDE_PROJECT (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_project_get_id (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_project_get_name (self));
      break;

    case PROP_ROOT:
      g_value_set_object (value, ide_project_get_root (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeProject *self = IDE_PROJECT (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      ide_project_set_root (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_class_init (IdeProjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_project_finalize;
  object_class->get_property = ide_project_get_property;
  object_class->set_property = ide_project_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "ID",
                         "The unique project identifier.",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name of the project.",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ROOT] =
    g_param_spec_object ("root",
                         "Root",
                         "The root object for the project.",
                         IDE_TYPE_PROJECT_ITEM,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [FILE_RENAMED] =
    g_signal_new ("file-renamed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_FILE, G_TYPE_FILE);

  signals [FILE_TRASHED] =
    g_signal_new ("file-trashed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_FILE);
}

static void
ide_project_init (IdeProject *self)
{
  g_rw_lock_init (&self->rw_lock);
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
  GTask *task = data;
  IdeProject *project;
  RenameFile *rf;

  g_assert (G_IS_TASK (task));

  project = g_task_get_source_object (task);
  rf = g_task_get_task_data (task);

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
ide_project_rename_file_worker (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  IdeProject *self = source_object;
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) parent = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  RenameFile *op = task_data;
  g_autoptr(GError) error = NULL;
  GFile *workdir;

  g_assert (IDE_IS_PROJECT (self));
  g_assert (op != NULL);
  g_assert (G_IS_FILE (op->orig_file));
  g_assert (G_IS_FILE (op->new_file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
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
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               _("Destination file must be within the project tree."));
      return;
    }

  parent = g_file_get_parent (op->new_file);

  if (!g_file_query_exists (parent, cancellable) &&
      !g_file_make_directory_with_parents (parent, cancellable, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
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
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_idle_add_full (G_PRIORITY_LOW,
                   emit_file_renamed,
                   g_object_ref (task),
                   g_object_unref);

  g_task_return_boolean (task, TRUE);
}

static void
ide_project_rename_buffer_save_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_buffer_manager_save_file_finish (bufmgr, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_run_in_thread (task, ide_project_rename_file_worker);
}

void
ide_project_rename_file_async (IdeProject          *self,
                               GFile               *orig_file,
                               GFile               *new_file,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeBufferManager *bufmgr;
  IdeContext *context;
  IdeBuffer *buffer;
  RenameFile *op;

  g_return_if_fail (IDE_IS_PROJECT (self));
  g_return_if_fail (G_IS_FILE (orig_file));
  g_return_if_fail (G_IS_FILE (new_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_project_rename_file_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  bufmgr = ide_context_get_buffer_manager (context);
  buffer = ide_buffer_manager_find_buffer (bufmgr, orig_file);

  op = g_slice_new0 (RenameFile);
  op->orig_file = g_object_ref (orig_file);
  op->new_file = g_object_ref (new_file);
  op->buffer = buffer ? g_object_ref (buffer) : NULL;
  g_task_set_task_data (task, op, rename_file_free);

  /*
   * If the file is open and has any changes, we need to save those
   * changes before we can proceed.
   */
  if (buffer != NULL && gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)))
    {
      g_autoptr(IdeFile) file = ide_file_new (context, orig_file);

      ide_buffer_manager_save_file_async (bufmgr,
                                          buffer,
                                          file,
                                          NULL,
                                          NULL,
                                          ide_project_rename_buffer_save_cb,
                                          g_steal_pointer (&task));
      return;
    }

  g_task_run_in_thread (task, ide_project_rename_file_worker);
}

gboolean
ide_project_rename_file_finish (IdeProject    *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_PROJECT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
ide_project_trash_file__file_trash_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeProject *self;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!g_file_trash_finish (file, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  self = g_task_get_source_object (task);

  g_assert (IDE_IS_PROJECT (self));

  g_signal_emit (self, signals [FILE_TRASHED], 0, file);
}

static void
ide_project_trash_file__wait_check_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeProject *self;
  GFile *file;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  self = g_task_get_source_object (task);
  file = g_task_get_task_data (task);

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
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_PROJECT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_project_trash_file_async);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);

  if (!file_is_ancestor (workdir, file))
    {
      g_task_return_new_error (task,
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
        g_task_return_error (task, g_steal_pointer (&error));
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
  GTask *task = (GTask *)result;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_PROJECT (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  ret = g_task_propagate_boolean (task, error);

  IDE_RETURN (ret);
}
