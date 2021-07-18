/* ide-session-addin.c
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

#define G_LOG_DOMAIN "ide-session-addin"

#include "config.h"

#include "ide-session-addin.h"

G_DEFINE_INTERFACE (IdeSessionAddin, ide_session_addin, IDE_TYPE_OBJECT)

static void
ide_session_addin_real_save_page_async (IdeSessionAddin     *self,
                                        IdePage             *page,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_session_addin_real_save_page_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Save not supported");
}

static GVariant *
ide_session_addin_real_save_page_finish (IdeSessionAddin  *self,
                                         GAsyncResult     *result,
                                         GError          **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_session_addin_real_restore_page_async (IdeSessionAddin     *self,
                                           GVariant            *state,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_session_addin_real_restore_page_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Restore not supported");
}

static IdePage *
ide_session_addin_real_restore_page_finish (IdeSessionAddin  *self,
                                            GAsyncResult     *result,
                                            GError          **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static gboolean
ide_session_addin_real_can_save_page (IdeSessionAddin *self,
                                      IdePage         *page)
{
  return FALSE;
}

static char **
ide_session_addin_real_get_autosave_properties (IdeSessionAddin *self)
{
  return NULL;
}

static void
ide_session_addin_default_init (IdeSessionAddinInterface *iface)
{
  iface->save_page_async = ide_session_addin_real_save_page_async;
  iface->save_page_finish = ide_session_addin_real_save_page_finish;
  iface->restore_page_async = ide_session_addin_real_restore_page_async;
  iface->restore_page_finish = ide_session_addin_real_restore_page_finish;
  iface->can_save_page = ide_session_addin_real_can_save_page;
  iface->get_autosave_properties = ide_session_addin_real_get_autosave_properties;
}

/**
 * ide_session_addin_save_page_async:
 * @self: a #IdeSessionAddin
 * @page: an #IdePage supported by ide_session_addin_can_save_page()
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronous request to save a page's session state.
 *
 * The addin implementation must not attempt to save the page's position within its
 * parent #IdeGrid. Instead it must only save how to restore the content of the page
 * (e.g. opening URI foo://path/to/file at position X:Y). It is the #IdeSession that
 * manages the position of the pages, which means session addins implementation can
 * be much simpler. See also ide_session_addin_save_page_finish().
 *
 * The resulting state will be provided when restoring the page
 * at a future time with ide_session_addin_restore_page_async().
 *
 * Since: 41
 */
void
ide_session_addin_save_page_async (IdeSessionAddin     *self,
                                   IdePage             *page,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SESSION_ADDIN (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SESSION_ADDIN_GET_IFACE (self)->save_page_async (self, page, cancellable, callback, user_data);
}

/**
 * ide_session_addin_save_page_finish:
 * @self: an #IdeSessionAddin
 *
 * Completes an asynchronous request to save a page's session state.
 *
 * The resulting #GVariant will be used to restore the page's state at a future time.
 * It is highly recommended to store the state as a vardict in the result variant, as
 * it's much more easy to expand the state later and to handle migrations if needed.
 * See also ide_session_addin_save_page_async().
 *
 * Returns: (transfer full) (nullable): a #GVariant or %NULL if an error prevented
 * from saving the page.
 *
 * Since: 41
 */
GVariant *
ide_session_addin_save_page_finish (IdeSessionAddin  *self,
                                    GAsyncResult     *result,
                                    GError          **error)
{
  g_return_val_if_fail (IDE_IS_SESSION_ADDIN (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_SESSION_ADDIN_GET_IFACE (self)->save_page_finish (self, result, error);
}

/**
 * ide_session_addin_restore_page_async:
 * @self: a #IdeSessionAddin
 * @state: a #GVariant of previous state
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that addin @self restore a page's session state with
 * the provided state, previously saved by this addin using
 * ide_session_addin_save_page_async(). This only happens when opening a project.
 *
 * Since: 41
 */
void
ide_session_addin_restore_page_async (IdeSessionAddin     *self,
                                      GVariant            *state,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SESSION_ADDIN (self));
  g_return_if_fail (state != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SESSION_ADDIN_GET_IFACE (self)->restore_page_async (self, state, cancellable, callback, user_data);
}

/**
 * ide_session_addin_restore_page_finish:
 * @self: a #IdeSessionAddin
 *
 * Completes an asynchronous request to restore a page's session state.
 *
 * Returns: (transfer full) (nullable): the created page for the saved state, or %NULL if an error
 * prevented from restoring the page.
 *
 * Since: 41
 */
IdePage *
ide_session_addin_restore_page_finish (IdeSessionAddin  *self,
                                       GAsyncResult     *result,
                                       GError          **error)
{
  g_return_val_if_fail (IDE_IS_SESSION_ADDIN (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_SESSION_ADDIN_GET_IFACE (self)->restore_page_finish (self, result, error);
}

/**
 * ide_session_addin_can_save_page:
 * @self: a #IdeSessionAddin
 * @page: an #IdePage
 *
 * Checks whether @self supports saving @page. This is typically done by checking for
 * its GObject type using `FOO_IS_BAR_PAGE ()` for page types defined in the plugin.
 * In practice it means that this @self addin supports all the different vfuncs for
 * this @page.
 *
 * Returns: whether @self supports saving @page.
 *
 * Since: 41
 */
gboolean
ide_session_addin_can_save_page (IdeSessionAddin *self,
                                 IdePage         *page)
{
  g_return_val_if_fail (IDE_IS_SESSION_ADDIN (self), FALSE);
  g_return_val_if_fail (IDE_IS_PAGE (page), FALSE);

  return IDE_SESSION_ADDIN_GET_IFACE (self)->can_save_page (self, page);
}

/**
 * ide_session_addin_get_autosave_properties:
 * @self: an #IdeSessionAddin
 *
 * For the pages supported by its ide_session_addin_can_save_page() function, gets
 * a list of properties names that should be watched for changes on this page using
 * the GObject notify mechanism. So given an array with "foo" and "bar", the #IdeSession
 * will connect to the "notify::foo" and "notify::bar" signals and schedule a saving
 * operation for some minutes later, so saving operations are grouped together.
 *
 * A possible autosave property could be the #IdePage's "title" property, in case
 * your state is always reflected there. But in general, it's better to use your
 * own custom page properties as it will be more reliable.
 *
 * Returns: (array zero-terminated=1) (element-type utf8) (nullable) (transfer full):
 *   A %NULL terminated array of properties names, or %NULL.
 *
 * Since: 41.0
 */
char **
ide_session_addin_get_autosave_properties (IdeSessionAddin *self)
{
  g_return_val_if_fail (IDE_IS_SESSION_ADDIN (self), NULL);

  return IDE_SESSION_ADDIN_GET_IFACE (self)->get_autosave_properties (self);
}
