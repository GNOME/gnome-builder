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

#include <libide-threading.h>

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
 *
 * Since: 3.32
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
 * @options: a #GVariantDict containing any user supplied options
 * @cancellable: (nullable): a #GCancellable
 * @progress: (nullable): a location for an #IdeNotification, or %NULL
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Since: 3.32
 */
void
ide_vcs_cloner_clone_async (IdeVcsCloner         *self,
                            const gchar          *uri,
                            const gchar          *destination,
                            GVariantDict         *options,
                            IdeNotification      *progress,
                            GCancellable         *cancellable,
                            GAsyncReadyCallback   callback,
                            gpointer              user_data)
{
  g_return_if_fail (IDE_IS_VCS_CLONER (self));
  g_return_if_fail (uri != NULL);
  g_return_if_fail (destination != NULL);
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
 *
 * Since: 3.32
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
 *
 * Since: 3.32
 */
gchar *
ide_vcs_cloner_get_title (IdeVcsCloner *self)
{
  g_return_val_if_fail (IDE_IS_VCS_CLONER (self), NULL);

  if (IDE_VCS_CLONER_GET_IFACE (self)->get_title)
    return IDE_VCS_CLONER_GET_IFACE (self)->get_title (self);

  return NULL;
}
