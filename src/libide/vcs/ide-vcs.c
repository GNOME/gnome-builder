/* ide-vcs.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-vcs"

#include "config.h"

#include <string.h>

#include "ide-context.h"

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-change-monitor.h"
#include "vcs/ide-vcs.h"

G_DEFINE_INTERFACE (IdeVcs, ide_vcs, IDE_TYPE_OBJECT)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];
static GPtrArray *ignored;

G_LOCK_DEFINE_STATIC (ignored);

void
ide_vcs_register_ignored (const gchar *pattern)
{
  G_LOCK (ignored);
  if (ignored == NULL)
    ignored = g_ptr_array_new ();
  g_ptr_array_add (ignored, g_pattern_spec_new (pattern));
  G_UNLOCK (ignored);
}

static void
ide_vcs_real_list_status_async (IdeVcs              *self,
                                GFile               *directory_or_file,
                                gboolean             include_descendants,
                                gint                 io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_vcs_real_list_status_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Not supported by %s",
                           G_OBJECT_TYPE_NAME (self));
}

static GListModel *
ide_vcs_real_list_status_finish (IdeVcs        *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_vcs_default_init (IdeVcsInterface *iface)
{
  iface->list_status_async = ide_vcs_real_list_status_async;
  iface->list_status_finish = ide_vcs_real_list_status_finish;

  g_object_interface_install_property (iface,
                                       g_param_spec_string ("branch-name",
                                                            "Branch Name",
                                                            "The current name of the branch",
                                                            NULL,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_object ("working-directory",
                                                            "Working Directory",
                                                            "The working directory for the VCS",
                                                            G_TYPE_FILE,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  /**
   * IdeVcs::changed:
   *
   * The "changed" signal should be emitted when the VCS has detected a change
   * to the underlying VCS storage. This can be used by consumers to reload
   * their respective data structures.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeVcsInterface, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [CHANGED],
                              G_TYPE_FROM_INTERFACE (iface),
                              g_cclosure_marshal_VOID__VOIDv);


  /* Ignore Gio temporary files */
  ide_vcs_register_ignored (".goutputstream-*");

  /* Ignore minified JS */
  ide_vcs_register_ignored ("*.min.js");
  ide_vcs_register_ignored ("*.min.js.*");
}

/**
 * ide_vcs_is_ignored:
 * @self: An #IdeVcs
 * @file: (nullable): a #GFile
 * @error: A location for a #GError, or %NULL
 *
 * This function will check if @file is considered an "ignored file" by
 * the underlying Version Control System.
 *
 * For convenience, this function will return %TRUE if @file is %NULL.
 *
 * If @self is %NULL, only static checks against known ignored files
 * will be performed (such as .git, .flatpak-builder, etc).
 *
 * Returns: %TRUE if the path should be ignored.
 *
 * Thread safety: This function is safe to call from a thread as
 *   #IdeVcs implementations are required to ensure this function
 *   is thread-safe.
 *
 * Since: 3.18
 */
gboolean
ide_vcs_is_ignored (IdeVcs  *self,
                    GFile   *file,
                    GError **error)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *reversed = NULL;
  gboolean ret = FALSE;
  gsize len;

  g_return_val_if_fail (!self || IDE_IS_VCS (self), FALSE);
  g_return_val_if_fail (!file || G_IS_FILE (file), FALSE);

  if (file == NULL)
    return TRUE;

  name = g_file_get_basename (file);
  if (name == NULL || *name == 0)
    return TRUE;

  len = strlen (name);

  /* Ignore builtin backup files by GIO */
  if (name[len - 1] == '~')
    return TRUE;

  reversed = g_utf8_strreverse (name, len);

  G_LOCK (ignored);

  if G_LIKELY (ignored != NULL)
    {
      for (guint i = 0; i < ignored->len; i++)
        {
          GPatternSpec *pattern_spec = g_ptr_array_index (ignored, i);

          if (g_pattern_match (pattern_spec, len, name, reversed))
            {
              ret = TRUE;
              break;
            }
        }
    }

  G_UNLOCK (ignored);

  if (self != NULL)
    {
      if (!ret && IDE_VCS_GET_IFACE (self)->is_ignored)
        ret = IDE_VCS_GET_IFACE (self)->is_ignored (self, file, error);
    }

  return ret;
}

/**
 * ide_vcs_path_is_ignored:
 * @self: An #IdeVcs
 * @path: (nullable): The path to check
 * @error: A location for a #GError, or %NULL
 *
 * This function acts like ide_vcs_is_ignored() except that it
 * allows for using a regular file-system path.
 *
 * It will check if the path is absolute or relative to the project
 * directory and adjust as necessary.
 *
 * For convenience, this function will return %TRUE if @path is %NULL.
 *
 * If @self is %NULL, only registered ignore patterns will be checked.
 *
 * Returns: %TRUE if the path should be ignored.
 *
 * Thread safety: This function is safe to call from a thread as
 *   #IdeVcs implementations are required to ensure this function
 *   is thread-safe.
 *
 * Since: 3.28
 */
gboolean
ide_vcs_path_is_ignored (IdeVcs       *self,
                         const gchar  *path,
                         GError      **error)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *reversed = NULL;
  gsize len;
  gboolean ret = FALSE;

  g_return_val_if_fail (!self || IDE_IS_VCS (self), FALSE);

  if (path == NULL)
    return TRUE;

  name = g_path_get_basename (path);
  if (name == NULL || *name == 0)
    return TRUE;

  len = strlen (name);

  /* Ignore builtin backup files by GIO */
  if (name[len - 1] == '~')
    return TRUE;

  reversed = g_utf8_strreverse (name, len);

  G_LOCK (ignored);

  if G_LIKELY (ignored != NULL)
    {
      for (guint i = 0; i < ignored->len; i++)
        {
          GPatternSpec *pattern_spec = g_ptr_array_index (ignored, i);

          if (g_pattern_match (pattern_spec, len, name, reversed))
            {
              ret = TRUE;
              break;
            }
        }
    }

  G_UNLOCK (ignored);

  if (self != NULL)
    {
      if (!ret && IDE_VCS_GET_IFACE (self)->is_ignored)
        {
          g_autoptr(GFile) file = NULL;

          if (g_path_is_absolute (path))
            file = g_file_new_for_path (path);
          else
            file = g_file_get_child (ide_vcs_get_working_directory (self), path);

          ret = IDE_VCS_GET_IFACE (self)->is_ignored (self, file, error);
        }
    }

  return ret;
}

gint
ide_vcs_get_priority (IdeVcs *self)
{
  gint ret = 0;

  g_return_val_if_fail (IDE_IS_VCS (self), 0);

  if (IDE_VCS_GET_IFACE (self)->get_priority)
    ret = IDE_VCS_GET_IFACE (self)->get_priority (self);

  return ret;
}

/**
 * ide_vcs_get_working_directory:
 * @self: An #IdeVcs.
 *
 * Retrieves the working directory for the context. This is the root of where
 * the project files exist.
 *
 * This function is safe to call from threads holding a reference to @self.
 *
 * Returns: (transfer none): a #GFile.
 *
 * Since: 3.18
 *
 * Thread safety: this function is safe to call from threads. The working
 *   directory should only be set at creating and therefore safe to call
 *   at any time from any thread that holds a reference to @self. Those
 *   implementing #IdeVcs are required to ensure this invariant holds true.
 */
GFile *
ide_vcs_get_working_directory (IdeVcs *self)
{
  g_return_val_if_fail (IDE_IS_VCS (self), NULL);

  if (IDE_VCS_GET_IFACE (self)->get_working_directory)
    return IDE_VCS_GET_IFACE (self)->get_working_directory (self);

  return NULL;
}

/**
 * ide_vcs_get_buffer_change_monitor:
 *
 * Gets an #IdeBufferChangeMonitor for the buffer provided. If the #IdeVcs implementation does not
 * support change monitoring, or cannot for the current file, then %NULL is returned.
 *
 * Returns: (transfer full) (nullable): An #IdeBufferChangeMonitor or %NULL.
 */
IdeBufferChangeMonitor *
ide_vcs_get_buffer_change_monitor (IdeVcs    *self,
                                   IdeBuffer *buffer)
{
  IdeBufferChangeMonitor *ret = NULL;

  g_return_val_if_fail (IDE_IS_VCS (self), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  if (IDE_VCS_GET_IFACE (self)->get_buffer_change_monitor)
    ret = IDE_VCS_GET_IFACE (self)->get_buffer_change_monitor (self, buffer);

  g_return_val_if_fail (!ret || IDE_IS_BUFFER_CHANGE_MONITOR (ret), NULL);

  return ret;
}

static gint
sort_by_priority (gconstpointer a,
                  gconstpointer b,
                  gpointer      user_data)
{
  IdeVcs *vcs_a = *(IdeVcs **)a;
  IdeVcs *vcs_b = *(IdeVcs **)b;

  return ide_vcs_get_priority (vcs_a) - ide_vcs_get_priority (vcs_b);
}

void
ide_vcs_new_async (IdeContext           *context,
                   int                   io_priority,
                   GCancellable         *cancellable,
                   GAsyncReadyCallback   callback,
                   gpointer              user_data)
{
  ide_object_new_for_extension_async (IDE_TYPE_VCS,
                                      sort_by_priority,
                                      NULL,
                                      io_priority,
                                      cancellable,
                                      callback,
                                      user_data,
                                      "context", context,
                                      NULL);
}

/**
 * ide_vcs_new_finish:
 *
 * Completes a call to ide_vcs_new_async().
 *
 * Returns: (transfer full): An #IdeVcs.
 */
IdeVcs *
ide_vcs_new_finish (GAsyncResult  *result,
                    GError       **error)
{
  IdeObject *ret;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = ide_object_new_finish (result, error);

  return IDE_VCS (ret);
}

void
ide_vcs_emit_changed (IdeVcs *self)
{
  g_return_if_fail (IDE_IS_VCS (self));

  g_signal_emit (self, signals [CHANGED], 0);
}

/**
 * ide_vcs_get_config:
 *
 * Retrieves an #IdeVcsConfig for the #IdeVcs provided. If the #IdeVcs implementation does not
 * support access to configuration, then %NULL is returned.
 *
 * Returns: (transfer full) (nullable): An #IdeVcsConfig or %NULL.
 */
IdeVcsConfig *
ide_vcs_get_config (IdeVcs *self)
{
  IdeVcsConfig *ret = NULL;

  g_return_val_if_fail (IDE_IS_VCS (self), NULL);

  if (IDE_VCS_GET_IFACE (self)->get_config)
    ret = IDE_VCS_GET_IFACE (self)->get_config (self);

  g_return_val_if_fail (!ret || IDE_IS_VCS_CONFIG (ret), NULL);

  return  ret;
}

/**
 * ide_vcs_get_branch_name:
 *
 * Retrieves the name of the branch in the current working directory.
 *
 * Returns: (transfer full): A string containing the branch name.
 */
gchar *
ide_vcs_get_branch_name (IdeVcs *self)
{
  g_return_val_if_fail (IDE_IS_VCS (self), NULL);

  if (IDE_VCS_GET_IFACE (self)->get_branch_name)
    return IDE_VCS_GET_IFACE (self)->get_branch_name (self);

  return g_strdup ("primary");
}

/**
 * ide_vcs_list_status_async:
 * @self: a #IdeVcs
 * @directory_or_file: a #GFile containing a file or directory within the
 *   working tree to retrieve the status of.
 * @include_descendants: if descendants of @directory_or_file should be
 *   included when retrieving status information.
 * @io_priority: a priority for the IO, such as %G_PRIORITY_DEFAULT.
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: a callback for the operation
 * @user_data: closure data for @callback
 *
 * Retrieves the status of the files matching the request. If
 * @directory_or_file is a directory, then all files within that directory
 * will be scanned for changes. If @include_descendants is %TRUE, the
 * #IdeVcs will scan sub-directories for changes as well.
 *
 * The function specified by @callback should call ide_vcs_list_status_finish()
 * to retrieve the result of this asynchronous operation.
 *
 * Since: 3.28
 */
void
ide_vcs_list_status_async (IdeVcs              *self,
                           GFile               *directory_or_file,
                           gboolean             include_descendants,
                           gint                 io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_return_if_fail (IDE_IS_VCS (self));
  g_return_if_fail (!directory_or_file || G_IS_FILE (directory_or_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (directory_or_file == NULL)
    directory_or_file = ide_vcs_get_working_directory (self);

  IDE_VCS_GET_IFACE (self)->list_status_async (self,
                                               directory_or_file,
                                               include_descendants,
                                               io_priority,
                                               cancellable,
                                               callback,
                                               user_data);
}

/**
 * ide_vcs_list_status_finish:
 * @self: a #IdeVcs
 * @result: a #GAsyncResult provided to the callback
 * @error: a location for a #GError
 *
 * Completes an asynchronous request to ide_vcs_list_status_async().
 *
 * The result of this function is a #GListModel containing objects that are
 * #IdeVcsFileInfo.
 *
 * Returns: (transfer full) (nullable):
 *   A #GListModel containing an #IdeVcsFileInfo for each of the files scanned
 *   by the #IdeVcs. Upon failure, %NULL is returned and @error is set.
 *
 * Since: 3.28
 */
GListModel *
ide_vcs_list_status_finish (IdeVcs        *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (IDE_IS_VCS (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_VCS_GET_IFACE (self)->list_status_finish (self, result, error);
}
