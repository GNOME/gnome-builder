/* ide-vcs-initializer.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-vcs-initializer"

#include "config.h"

#include "ide-vcs-initializer.h"

G_DEFINE_INTERFACE (IdeVcsInitializer, ide_vcs_initializer, IDE_TYPE_OBJECT)

static void
ide_vcs_initializer_default_init (IdeVcsInitializerInterface *iface)
{
}

void
ide_vcs_initializer_initialize_async  (IdeVcsInitializer   *self,
                                       GFile               *file,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_return_if_fail (IDE_IS_VCS_INITIALIZER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_VCS_INITIALIZER_GET_IFACE (self)->initialize_async (self, file, cancellable, callback, user_data);
}

gboolean
ide_vcs_initializer_initialize_finish (IdeVcsInitializer  *self,
                                       GAsyncResult       *result,
                                       GError            **error)
{
  g_return_val_if_fail (IDE_IS_VCS_INITIALIZER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_VCS_INITIALIZER_GET_IFACE (self)->initialize_finish (self, result, error);
}

gchar *
ide_vcs_initializer_get_title (IdeVcsInitializer *self)
{
  g_return_val_if_fail (IDE_IS_VCS_INITIALIZER (self), NULL);

  if (IDE_VCS_INITIALIZER_GET_IFACE (self)->get_title)
    return IDE_VCS_INITIALIZER_GET_IFACE (self)->get_title (self);

  return g_strdup (G_OBJECT_TYPE_NAME (self));
}
