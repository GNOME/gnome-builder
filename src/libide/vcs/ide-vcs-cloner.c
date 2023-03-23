/* ide-vcs-cloner.c
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

#define G_LOG_DOMAIN "ide-vcs-cloner"

#include "config.h"

#include <gdk/gdk.h>

#include <libide-threading.h>
#include <libpeas.h>

#include "ide-vcs-cloner.h"

G_DEFINE_INTERFACE (IdeVcsCloner, ide_vcs_cloner, IDE_TYPE_OBJECT)

static void
ide_vcs_cloner_default_init (IdeVcsClonerInterface *iface)
{
}

/**
 * ide_vcs_cloner_validate_uri:
 * @self: a #IdeVcsCloner
 * @uri: a string containing the URI to validate
 * @errmsg: (out) (optional): a location for an error message
 *
 * Checks to see if @uri is valid, and if not, sets @errmsg to a string
 * describing how the URI is invalid.
 *
 * Returns: %TRUE if @uri is valid, otherwise %FALSE and @error is set.
 */
gboolean
ide_vcs_cloner_validate_uri (IdeVcsCloner  *self,
                             const gchar   *uri,
                             gchar        **errmsg)
{
  g_return_val_if_fail (IDE_IS_VCS_CLONER (self), FALSE);

  if (errmsg != NULL)
    *errmsg = NULL;

  if (IDE_VCS_CLONER_GET_IFACE (self)->validate_uri)
    return IDE_VCS_CLONER_GET_IFACE (self)->validate_uri (self, uri, errmsg);

  return FALSE;
}

/**
 * ide_vcs_cloner_clone_async:
 * @self: an #IdeVcsCloner
 * @uri: a string containing the URI
 * @destination: a string containing the destination path
 * @options: a #GVariant containing any user supplied options
 * @cancellable: (nullable): a #GCancellable
 * @progress: (nullable): a location for an #IdeNotification, or %NULL
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 */
void
ide_vcs_cloner_clone_async (IdeVcsCloner         *self,
                            const gchar          *uri,
                            const gchar          *destination,
                            GVariant             *options,
                            IdeNotification      *progress,
                            GCancellable         *cancellable,
                            GAsyncReadyCallback   callback,
                            gpointer              user_data)
{
  g_return_if_fail (IDE_IS_VCS_CLONER (self));
  g_return_if_fail (uri != NULL);
  g_return_if_fail (destination != NULL);
  g_return_if_fail (options != NULL);
  g_return_if_fail (g_variant_is_of_type (options, G_VARIANT_TYPE_VARDICT));
  g_return_if_fail (!progress || IDE_IS_NOTIFICATION (progress));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_VCS_CLONER_GET_IFACE (self)->clone_async (self,
                                                uri,
                                                destination,
                                                options,
                                                progress,
                                                cancellable,
                                                callback,
                                                user_data);
}

/**
 * ide_vcs_cloner_clone_finish:
 * @self: an #IdeVcsCloner
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_vcs_cloner_clone_finish (IdeVcsCloner  *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (IDE_IS_VCS_CLONER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_VCS_CLONER_GET_IFACE (self)->clone_finish (self, result, error);
}

/**
 * ide_vcs_cloner_get_title:
 * @self: a #IdeVcsCloner
 *
 * Gets the for the cloner, such as "Git". This may be used to present
 * a selector to the user based on the backend clone engine. Other suitable
 * titles might be "Subversion" or "CVS".
 *
 * Returns: (transfer full): a string containing the title
 */
gchar *
ide_vcs_cloner_get_title (IdeVcsCloner *self)
{
  g_return_val_if_fail (IDE_IS_VCS_CLONER (self), NULL);

  if (IDE_VCS_CLONER_GET_IFACE (self)->get_title)
    return IDE_VCS_CLONER_GET_IFACE (self)->get_title (self);

  return NULL;
}

typedef struct
{
  GMutex           mutex;
  GCond            cond;
  IdeContext      *context;
  const gchar     *module_name;
  const gchar     *url;
  const gchar     *branch;
  const gchar     *destination;
  IdeNotification *notif;
  GCancellable    *cancellable;
  GError          *error;
} CloneSimple;

static void
ide_vcs_cloner_clone_simple_clone_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeVcsCloner *cloner = (IdeVcsCloner *)object;
  CloneSimple *state = user_data;

  g_assert (IDE_IS_VCS_CLONER (cloner));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state->module_name != NULL);
  g_assert (state->url != NULL);
  g_assert (state->destination != NULL);
  g_assert (!state->notif || IDE_IS_NOTIFICATION (state->notif));
  g_assert (!state->cancellable || IDE_IS_NOTIFICATION (state->cancellable));
  g_assert (state->error == NULL);

  ide_vcs_cloner_clone_finish (cloner, result, &state->error);

  g_mutex_lock (&state->mutex);
  g_cond_signal (&state->cond);
  g_mutex_unlock (&state->mutex);
}

static gboolean
ide_vcs_cloner_clone_simple_idle_cb (CloneSimple *state)
{
  g_autoptr(GObject) exten = NULL;
  g_autoptr(GVariant) options = NULL;
  PeasPluginInfo *plugin_info;
  PeasEngine *engine;
  GVariantDict dict;

  g_assert (state != NULL);
  g_assert (IDE_IS_CONTEXT (state->context));
  g_assert (state->module_name != NULL);
  g_assert (state->url != NULL);
  g_assert (state->destination != NULL);
  g_assert (!state->notif || IDE_IS_NOTIFICATION (state->notif));
  g_assert (!state->cancellable || IDE_IS_NOTIFICATION (state->cancellable));
  g_assert (state->error == NULL);

  engine = peas_engine_get_default ();

  if (!(plugin_info = peas_engine_get_plugin_info (engine, state->module_name)))
    {
      g_set_error (&state->error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "No such module %s",
                   state->module_name);
      goto notify;
    }

  exten = peas_engine_create_extension (engine,
                                        plugin_info,
                                        IDE_TYPE_VCS_CLONER,
                                        "parent", state->context,
                                        NULL);

  if (exten == NULL)
    {
      g_set_error (&state->error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Failed to create IdeVcsCloner from module %s",
                   state->module_name);
      goto notify;
    }

  g_variant_dict_init (&dict, NULL);
  if (state->branch != NULL)
    g_variant_dict_insert (&dict, "branch", "s", state->branch);
  options = g_variant_take_ref (g_variant_dict_end (&dict));

  ide_vcs_cloner_clone_async (IDE_VCS_CLONER (exten),
                              state->url,
                              state->destination,
                              options,
                              state->notif,
                              state->cancellable,
                              ide_vcs_cloner_clone_simple_clone_cb,
                              state);

  return G_SOURCE_REMOVE;

notify:

  g_mutex_lock (&state->mutex);
  g_cond_signal (&state->cond);
  g_mutex_unlock (&state->mutex);

  return G_SOURCE_REMOVE;
}

gboolean
ide_vcs_cloner_clone_simple (IdeContext       *context,
                             const gchar      *module_name,
                             const gchar      *url,
                             const gchar      *branch,
                             const gchar      *destination,
                             IdeNotification  *notif,
                             GCancellable     *cancellable,
                             GError          **error)
{
  CloneSimple state = {
    .context = context,
    .module_name = module_name,
    .url = url,
    .branch = branch,
    .destination = destination,
    .notif = notif,
    .cancellable = cancellable,
    .error = NULL,
  };

  g_return_val_if_fail (!IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_CONTEXT (context), FALSE);
  g_return_val_if_fail (module_name != NULL, FALSE);
  g_return_val_if_fail (url != NULL, FALSE);
  g_return_val_if_fail (destination != NULL, FALSE);
  g_return_val_if_fail (!notif || IDE_IS_NOTIFICATION (notif), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  g_mutex_init (&state.mutex);
  g_cond_init (&state.cond);

  g_mutex_lock (&state.mutex);

  g_idle_add_full (G_PRIORITY_HIGH,
                   (GSourceFunc) ide_vcs_cloner_clone_simple_idle_cb,
                   &state, NULL);

  g_cond_wait (&state.cond, &state.mutex);
  g_mutex_unlock (&state.mutex);
  g_cond_clear (&state.cond);
  g_mutex_clear (&state.mutex);

  if (state.error != NULL)
    {
      g_propagate_error (error, g_steal_pointer (&state.error));
      return FALSE;
    }

  return TRUE;
}

void
ide_vcs_cloner_list_branches_async (IdeVcsCloner        *self,
                                    IdeVcsUri           *uri,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_VCS_CLONER (self));
  g_return_if_fail (uri != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_VCS_CLONER_GET_IFACE (self)->list_branches_async (self, uri, cancellable, callback, user_data);

  IDE_EXIT;
}

/**
 * ide_vcs_cloner_list_branches_finish:
 *
 * Returns: (transfer full): a #GListModel of #IdeVcsBranch
 */
GListModel *
ide_vcs_cloner_list_branches_finish (IdeVcsCloner  *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_VCS_CLONER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = IDE_VCS_CLONER_GET_IFACE (self)->list_branches_finish (self, result, error);

  g_return_val_if_fail (!ret || G_IS_LIST_MODEL (ret), NULL);

  IDE_RETURN (ret);
}

/**
 * ide_vcs_cloner_get_directory_name:
 * @self: a #IdeVcsCloner
 * @uri: an #IdeVcsUri
 *
 * Gets the directory name that will be used to clone from @uri.
 *
 * If the path has "foo.git", this function would be expected to
 * return "foo".
 *
 * Returns: (transfer full): a string containing the directory name
 */
char *
ide_vcs_cloner_get_directory_name (IdeVcsCloner *self,
                                   IdeVcsUri    *uri)
{
  char *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_VCS_CLONER (self), NULL);
  g_return_val_if_fail (uri != NULL, NULL);

  ret = IDE_VCS_CLONER_GET_IFACE (self)->get_directory_name (self, uri);

  IDE_RETURN (ret);
}

/**
 * ide_vcs_cloner_set_pty_fd:
 * @self: a #IdeVcsCloner
 * @pty_fd: a fd or -1
 *
 * Sets a PTY that should be written to for message contents.
 *
 * Since: 44
 */
void
ide_vcs_cloner_set_pty_fd (IdeVcsCloner *self,
                           int           pty_fd)
{
  g_return_if_fail (IDE_IS_VCS_CLONER (self));

  if (IDE_VCS_CLONER_GET_IFACE (self)->set_pty_fd)
    IDE_VCS_CLONER_GET_IFACE (self)->set_pty_fd (self, pty_fd);
}
