/* ide-rename-provider.h
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

#pragma once

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_RENAME_PROVIDER (ide_rename_provider_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeRenameProvider, ide_rename_provider, IDE, RENAME_PROVIDER, IdeObject)

struct _IdeRenameProviderInterface
{
  GTypeInterface parent_iface;

  void     (*load)          (IdeRenameProvider    *self);
  void     (*unload)        (IdeRenameProvider    *self);
  void     (*rename_async)  (IdeRenameProvider    *self,
                             IdeLocation          *location,
                             const gchar          *new_name,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data);
  gboolean (*rename_finish) (IdeRenameProvider    *self,
                             GAsyncResult         *result,
                             GPtrArray           **edits,
                             GError              **error);
};

IDE_AVAILABLE_IN_ALL
void     ide_rename_provider_load          (IdeRenameProvider    *self);
IDE_AVAILABLE_IN_ALL
void     ide_rename_provider_unload        (IdeRenameProvider    *self);
IDE_AVAILABLE_IN_ALL
void     ide_rename_provider_rename_async  (IdeRenameProvider    *self,
                                            IdeLocation          *location,
                                            const gchar          *new_name,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_rename_provider_rename_finish (IdeRenameProvider    *self,
                                            GAsyncResult         *result,
                                            GPtrArray           **edits,
                                            GError              **error);

G_END_DECLS
