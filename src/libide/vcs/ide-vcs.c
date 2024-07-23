/* ide-vcs.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-vcs"

#include "config.h"

#include <string.h>
#include <glib/gi18n.h>

#include <libide-io.h>

#include "ide-directory-vcs.h"
#include "ide-vcs.h"
#include "ide-vcs-enums.h"

G_DEFINE_INTERFACE (IdeVcs, ide_vcs, IDE_TYPE_OBJECT)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

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
ide_vcs_real_list_branches_async (IdeVcs              *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_vcs_real_list_branches_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Not supported by %s",
                           G_OBJECT_TYPE_NAME (self));
}

static GPtrArray *
ide_vcs_real_list_branches_finish (IdeVcs        *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_vcs_real_list_tags_async (IdeVcs              *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_vcs_real_list_tags_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Not supported by %s",
                           G_OBJECT_TYPE_NAME (self));
}

static GPtrArray *
ide_vcs_real_list_tags_finish (IdeVcs        *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_vcs_real_switch_branch_async (IdeVcs              *self,
                                  IdeVcsBranch        *branch,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_vcs_real_switch_branch_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Not supported by %s",
                           G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_vcs_real_switch_branch_finish (IdeVcs        *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_vcs_real_push_branch_async (IdeVcs              *self,
                                IdeVcsBranch        *branch,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_vcs_real_push_branch_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Not supported by %s",
                           G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_vcs_real_push_branch_finish (IdeVcs        *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_vcs_default_init (IdeVcsInterface *iface)
{
  iface->list_status_async = ide_vcs_real_list_status_async;
  iface->list_status_finish = ide_vcs_real_list_status_finish;
  iface->list_branches_async = ide_vcs_real_list_branches_async;
  iface->list_branches_finish = ide_vcs_real_list_branches_finish;
  iface->list_tags_async = ide_vcs_real_list_tags_async;
  iface->list_tags_finish = ide_vcs_real_list_tags_finish;
  iface->switch_branch_async = ide_vcs_real_switch_branch_async;
  iface->switch_branch_finish = ide_vcs_real_switch_branch_finish;
  iface->push_branch_async = ide_vcs_real_push_branch_async;
  iface->push_branch_finish = ide_vcs_real_push_branch_finish;

  g_object_interface_install_property (iface,
                                       g_param_spec_string ("branch-name",
                                                            "Branch Name",
                                                            "The current name of the branch",
                                                            NULL,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_object ("workdir",
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
 */
gboolean
ide_vcs_is_ignored (IdeVcs  *self,
                    GFile   *file,
                    GError **error)
{
  g_return_val_if_fail (!self || IDE_IS_VCS (self), FALSE);
  g_return_val_if_fail (!file || G_IS_FILE (file), FALSE);

  if (file == NULL)
    return TRUE;

  if (ide_g_file_is_ignored (file))
    return TRUE;

  if (self != NULL)
    {
      if (IDE_VCS_GET_IFACE (self)->is_ignored)
        return IDE_VCS_GET_IFACE (self)->is_ignored (self, file, error);
    }

  return FALSE;
}

/**
 * ide_vcs_query_ignored:
 * @self: a #IdeVcs
 *
 * Returns: (transfer full): a #DexFuture to a gboolean
 *
 * Since: 47
 */
DexFuture *
ide_vcs_query_ignored (IdeVcs *self,
                       GFile  *file)
{
  g_return_val_if_fail (!self || IDE_IS_VCS (self), NULL);
  g_return_val_if_fail (!file || G_IS_FILE (file), NULL);

  if (file == NULL)
    return dex_future_new_for_boolean (TRUE);

  if (ide_g_file_is_ignored (file))
    return dex_future_new_for_boolean (TRUE);

  if (self == NULL)
    return dex_future_new_for_boolean (FALSE);

  if (IDE_VCS_GET_IFACE (self)->query_ignored == NULL)
    {
      GError *error = NULL;
      gboolean ret;

      ret = ide_vcs_is_ignored (self, file, NULL);

      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));
      else
        return dex_future_new_for_boolean (ret);
    }

  return IDE_VCS_GET_IFACE (self)->query_ignored (self, file);
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
 */
gboolean
ide_vcs_path_is_ignored (IdeVcs       *self,
                         const gchar  *path,
                         GError      **error)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (!self || IDE_IS_VCS (self), FALSE);

  if (path == NULL)
    return TRUE;

  if (ide_path_is_ignored (path))
    return TRUE;

  if (self != NULL)
    {
      if (!ret && IDE_VCS_GET_IFACE (self)->is_ignored)
        {
          g_autoptr(GFile) file = NULL;

          if (g_path_is_absolute (path))
            file = g_file_new_for_path (path);
          else
            file = g_file_get_child (ide_vcs_get_workdir (self), path);

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
 * ide_vcs_get_workdir:
 * @self: An #IdeVcs.
 *
 * Retrieves the working directory for the context. This is the root of where
 * the project files exist.
 *
 * This function is safe to call from threads holding a reference to @self.
 *
 * Returns: (transfer none): a #GFile.
 *
 * Thread safety: this function is safe to call from threads. The working
 *   directory should only be set at creating and therefore safe to call
 *   at any time from any thread that holds a reference to @self. Those
 *   implementing #IdeVcs are required to ensure this invariant holds true.
 */
GFile *
ide_vcs_get_workdir (IdeVcs *self)
{
  g_return_val_if_fail (IDE_IS_VCS (self), NULL);

  if (IDE_VCS_GET_IFACE (self)->get_workdir)
    return IDE_VCS_GET_IFACE (self)->get_workdir (self);

  return NULL;
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
  gchar *ret = NULL;

  g_return_val_if_fail (IDE_IS_VCS (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  if (IDE_VCS_GET_IFACE (self)->get_branch_name)
    ret = IDE_VCS_GET_IFACE (self)->get_branch_name (self);
  ide_object_unlock (IDE_OBJECT (self));

  if (ret == NULL)
    ret = g_strdup ("primary");

  return ret;
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
    directory_or_file = ide_vcs_get_workdir (self);

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

/**
 * ide_vcs_from_context:
 * @context: an #IdeContext
 *
 * Gets the #IdeVcs for the context.
 *
 * Returns: (transfer none): an #IdeVcs
 */
IdeVcs *
ide_vcs_from_context (IdeContext *context)
{
  IdeVcs *ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  /* Release full reference, into borrowed ref */
  ret = ide_vcs_ref_from_context (context);
  g_object_unref (ret);

  return ret;
}

/**
 * ide_vcs_ref_from_context:
 * @context: an #IdeContext
 *
 * A thread-safe version of ide_vcs_from_context().
 *
 * Returns: (transfer full): an #IdeVcs
 */
IdeVcs *
ide_vcs_ref_from_context (IdeContext *context)
{
  IdeVcs *ret;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  ide_object_lock (IDE_OBJECT (context));
  ret = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_VCS);
  if (ret == NULL)
    {
      g_autoptr(GFile) workdir = ide_context_ref_workdir (context);
      ret = (IdeVcs *)ide_directory_vcs_new (workdir);
      ide_object_prepend (IDE_OBJECT (context), IDE_OBJECT (ret));
    }
  ide_object_unlock (IDE_OBJECT (context));

  return g_steal_pointer (&ret);
}

void
ide_vcs_list_branches_async (IdeVcs              *self,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_return_if_fail (IDE_IS_VCS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_VCS_GET_IFACE (self)->list_branches_async (self, cancellable, callback, user_data);
}

/**
 * ide_vcs_list_branches_finish:
 * @self: an #IdeVcs
 * @result: a #GAsyncResult
 * @error: location for a #GError
 *
 * Returns: (transfer full) (element-type IdeVcsBranch): an array of
 *   #IdeVcsBranch.
 */
GPtrArray *
ide_vcs_list_branches_finish (IdeVcs        *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_VCS (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_VCS_GET_IFACE (self)->list_branches_finish (self, result, error);
}

void
ide_vcs_list_tags_async (IdeVcs              *self,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  g_return_if_fail (IDE_IS_VCS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_VCS_GET_IFACE (self)->list_tags_async (self, cancellable, callback, user_data);
}

/**
 * ide_vcs_list_tags_finish:
 * @self: an #IdeVcs
 * @result: a #GAsyncResult
 * @error: location for a #GError
 *
 * Returns: (transfer full) (element-type IdeVcsBranch): an array of
 *   #IdeVcsBranch.
 */
GPtrArray *
ide_vcs_list_tags_finish (IdeVcs        *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_VCS (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_VCS_GET_IFACE (self)->list_tags_finish (self, result, error);
}

void
ide_vcs_switch_branch_async (IdeVcs              *self,
                             IdeVcsBranch        *branch,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_VCS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_VCS_GET_IFACE (self)->switch_branch_async (self, branch, cancellable, callback, user_data);
}

gboolean
ide_vcs_switch_branch_finish (IdeVcs        *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_VCS (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_VCS_GET_IFACE (self)->switch_branch_finish (self, result, error);
}

void
ide_vcs_push_branch_async (IdeVcs              *self,
                           IdeVcsBranch        *branch,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_VCS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_VCS_GET_IFACE (self)->push_branch_async (self, branch, cancellable, callback, user_data);
}

gboolean
ide_vcs_push_branch_finish (IdeVcs        *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_VCS (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_VCS_GET_IFACE (self)->push_branch_finish (self, result, error);
}

/**
 * ide_vcs_get_display_name:
 * @self: a #IdeVcs
 *
 * Gets the display name for the VCS.
 *
 * Returns: (transfer full): a string describing the VCS
 */
char *
ide_vcs_get_display_name (IdeVcs *self)
{
  g_return_val_if_fail (IDE_IS_VCS (self), NULL);

  if (IDE_VCS_GET_IFACE (self)->get_display_name == NULL)
    /* translators: None means "no version control system" */
    return g_strdup (_("None"));

  return IDE_VCS_GET_IFACE (self)->get_display_name (self);
}
