/*
 * ide-fold-provider.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "ide-fold-provider.h"

G_DEFINE_ABSTRACT_TYPE (IdeFoldProvider, ide_fold_provider, IDE_TYPE_OBJECT)

static void
ide_fold_provider_class_init (IdeFoldProviderClass *klass)
{
}

static void
ide_fold_provider_init (IdeFoldProvider *self)
{
}

void
ide_fold_provider_list_regions_async (IdeFoldProvider     *self,
                                      IdeBuffer           *buffer,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_return_if_fail (IDE_IS_FOLD_PROVIDER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_FOLD_PROVIDER_GET_CLASS (self)->list_regions_async (self, buffer, cancellable, callback, user_data);
}

/**
 * ide_fold_provider_list_regions_finish:
 *
 * Returns: (transfer full): an #IdeFoldRegions if successful; otherwise
 *   %NULL and @error is set.
 *
 * Since: 47
 */
IdeFoldRegions *
ide_fold_provider_list_regions_finish (IdeFoldProvider  *self,
                                       GAsyncResult     *result,
                                       GError          **error)
{
  IdeFoldRegions *ret;

  g_return_val_if_fail (IDE_IS_FOLD_PROVIDER (self), NULL);

  ret = IDE_FOLD_PROVIDER_GET_CLASS (self)->list_regions_finish (self, result, error);

  g_return_val_if_fail (!ret || IDE_IS_FOLD_REGIONS (ret), NULL);

  return ret;
}
