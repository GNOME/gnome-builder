/* ide-load-directory-task.c
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

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-load-directory-task.h"
#include "ide-project.h"
#include "ide-project-item.h"
#include "ide-project-file.h"
#include "ide-project-files.h"
#include "ide-vcs.h"

#define DEFAULT_MAX_FILES 15000

typedef struct
{
  GTask          *task;
  IdeContext     *context;
  GCancellable   *cancellable;
  IdeProjectItem *parent;
  GFile          *directory;
  GHashTable     *directories;
  int             io_priority;
  gsize           max_files;
  gsize           current_files;
  guint           top_is_native : 1;
} IdeLoadDirectoryTask;

static void
ide_load_directory_task_free (gpointer data)
{
  IdeLoadDirectoryTask *state = data;

  if (state)
    {
      ide_clear_weak_pointer (&state->task);
      g_clear_pointer (&state->directories, g_hash_table_unref);
      g_clear_object (&state->cancellable);
      g_clear_object (&state->directory);
      g_clear_object (&state->parent);
      g_clear_object (&state->context);
      g_free (state);
    }
}

static gboolean
is_home_directory (GFile *directory)
{
  g_autofree gchar *path = NULL;

  g_assert (G_IS_FILE (directory));

  path = g_file_get_path (directory);
  return (g_strcmp0 (path, g_get_home_dir ()) == 0);
}

static gboolean
ide_load_directory_task_load_directory (IdeLoadDirectoryTask  *self,
                                        GFile                 *directory,
                                        GError               **error)
{
  g_autoptr(GFileEnumerator) children = NULL;
  g_autoptr(GPtrArray) directories = NULL;
  GFileInfo *child_info;
  IdeVcs *vcs;
  GFileType file_type;
  GError *local_error = NULL;
  gsize i;

  g_assert (self);
  g_assert (G_IS_FILE (directory));
  g_assert (error);

  /* ignore diving into this directory if we reached max files */
  if (self->current_files > self->max_files)
    return TRUE;

  /*
   * Ensure we are working with a directory.
   */
  file_type = g_file_query_file_type (self->directory, G_FILE_QUERY_INFO_NONE, self->cancellable);
  if (file_type != G_FILE_TYPE_DIRECTORY)
    {
      g_autofree gchar *path = NULL;

      path = g_file_get_path (self->directory);
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_DIRECTORY,
                   _("\"%s\" is not a directory."),
                   path);
      return FALSE;
    }

  /*
   * If this directory is non-native (including SSHFS FUSE mounts), don't try
   * to enumerate the children. However, it is okay to recurse if the top-level
   * directory is also non-native. This could easily happen in a situation like
   * sshfs to your server with HTML files.
   */
  if (self->top_is_native && !g_file_is_native (directory))
    return TRUE;

  /*
   * Get an enumerator for children in this directory.
   */
  children = g_file_enumerate_children (directory,
                                        G_FILE_ATTRIBUTE_STANDARD_NAME","
                                        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                                        G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                        G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE,
                                        G_FILE_QUERY_INFO_NONE,
                                        self->cancellable,
                                        error);
  if (!children)
    return FALSE;

  /*
   * Get a handle to our vcs, which is used to check for ignored files.
   */
  vcs = ide_context_get_vcs (self->context);

  /*
   * Walk the children to inflate their IdeProjectFile instances.
   */
  while ((child_info = g_file_enumerator_next_file (children, self->cancellable, &local_error)))
    {
      g_autoptr(IdeProjectItem) item = NULL;
      g_autoptr(GFile) file = NULL;
      g_autofree gchar *path = NULL;
      const gchar *name;

      name = g_file_info_get_attribute_byte_string (child_info, G_FILE_ATTRIBUTE_STANDARD_NAME);
      file_type = g_file_info_get_attribute_uint32 (child_info, G_FILE_ATTRIBUTE_STANDARD_TYPE);

      switch (file_type)
        {
        case G_FILE_TYPE_DIRECTORY:
        case G_FILE_TYPE_REGULAR:
          file = g_file_get_child (directory, name);

          /* check for known ignored files */
          if (ide_vcs_is_ignored (vcs, file, NULL))
            break;

          /* add the file item to the project tree */
          path = g_file_get_relative_path (self->directory, file);
          item = g_object_new (IDE_TYPE_PROJECT_FILE,
                               "context", self->context,
                               "file", file,
                               "file-info", child_info,
                               "parent", NULL,
                               "path", path,
                               NULL);

          ide_project_files_add_file (IDE_PROJECT_FILES (self->parent), IDE_PROJECT_FILE (item));
          self->current_files++;

          if (file_type == G_FILE_TYPE_DIRECTORY)
            {
              if (directories == NULL)
                directories = g_ptr_array_new_with_free_func (g_object_unref);
              g_ptr_array_add (directories, g_object_ref (file));
            }

          break;

        case G_FILE_TYPE_MOUNTABLE:
        case G_FILE_TYPE_SHORTCUT:
        case G_FILE_TYPE_SPECIAL:
        case G_FILE_TYPE_SYMBOLIC_LINK:
        case G_FILE_TYPE_UNKNOWN:
        default:
          break;
        }

      g_object_unref (child_info);
    }

  /*
   * Propagate error if necessary.
   */
  if (local_error)
    {
      g_propagate_error (error, local_error);
      return FALSE;
    }

  /*
   * Close the enumerator immediately so we don't hold onto resources while
   * traversing deeper in the directory structure.
   */
  if (!g_file_enumerator_is_closed (children) &&
      !g_file_enumerator_close (children, self->cancellable, error))
    return FALSE;

  /*
   * Now load all of the directories we found at this level.
   */
  if (directories)
    {
      for (i = 0; i < directories->len; i++)
        {
          GFile *file = g_ptr_array_index (directories, i);

          if (!ide_load_directory_task_load_directory (self, file, error))
            return FALSE;
        }
    }

  return TRUE;
}

static void
ide_load_directory_task_worker (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  IdeLoadDirectoryTask *self = task_data;
  IdeProject *project;
  GError *error = NULL;

  g_assert (self);
  g_assert (self->task == task);
  g_assert (G_IS_FILE (self->directory));

  project = ide_context_get_project (self->context);

  /*
   * If this is the users home directory, let's cheat and use the Projects
   * directory if there is one. Ideally, users wouldn't be opening their
   * Home directory as the project directory, but it could happen.
   */
  if (is_home_directory (self->directory))
    {
      g_autoptr(GFile) child = NULL;

      child = g_file_get_child (self->directory, "Projects");
      if (g_file_query_exists (child, cancellable))
        g_set_object (&self->directory, child);
    }

  ide_project_writer_lock (project);
  if (!ide_load_directory_task_load_directory (self, self->directory, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
  ide_project_writer_unlock (project);
}

/**
 * ide_load_directory_task_new:
 * @source_object: (allow-none) (ctype GObject*): the owning #GObject
 * @directory: A #GFile for the containing directory.
 * @parent: The root #IdeProjectItem to add the files to.
 * @max_files: The max number of files to discover.
 * @io_priority: an priority such as %G_PRIORITY_DEFAULT.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: (scope async): a callback to execute upon completion
 * @user_data: user data for @callback.
 *
 * This creates a new threaded task that will walk the filesystem for files
 * starting from the directory specified by @directory. Only @max_files files
 * will be loaded, which helps in situations where the user has specified a
 * very large directory structure such as their home directory.
 *
 * Some effort will be done to avoid directories that we are fairly certain
 * we should avoid (.git, .svn, Music, Pictures, etc).
 *
 * The @max_files parameter is treated lazily. It is only checked when
 * entering a directory. Therefore, more than @max_files may be loaded in an
 * attempt to preserve the overall consistency within a directory. That means
 * you will not have partial loads of a directory, but may not see descendents
 * within some child directories.
 *
 * Returns: (transfer full): A newly allocated #GTask that will return a
 *  gboolean if successful. Retrieve the result with
 *  g_task_propagate_boolean().
 */
GTask *
ide_load_directory_task_new (gpointer             source_object,
                             GFile               *directory,
                             IdeProjectItem      *parent,
                             gsize                max_files,
                             int                  io_priority,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  IdeLoadDirectoryTask *state;
  IdeContext *context;
  GTask *task;

  g_return_val_if_fail (!source_object || G_IS_OBJECT (source_object), NULL);
  g_return_val_if_fail (G_IS_FILE (directory), NULL);
  g_return_val_if_fail (IDE_IS_PROJECT_ITEM (parent), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  context = ide_object_get_context (IDE_OBJECT (parent));

  task = g_task_new (source_object, cancellable, callback, user_data);

  state = g_new0 (IdeLoadDirectoryTask, 1);
  ide_set_weak_pointer (&state->task, task);
  state->context = g_object_ref (context);
  state->directories = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal,
                                              g_object_unref, g_object_unref);
  state->directory = g_object_ref (directory);
  state->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  state->parent = g_object_ref (parent);
  state->io_priority = io_priority;
  state->max_files = max_files ?: DEFAULT_MAX_FILES;
  state->current_files = 0;
  state->top_is_native = g_file_is_native (directory);

  g_task_set_task_data (task, state, ide_load_directory_task_free);
  g_task_run_in_thread (task, ide_load_directory_task_worker);

  return task;
}
