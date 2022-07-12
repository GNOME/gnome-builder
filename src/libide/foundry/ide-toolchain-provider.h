/* ide-toolchain-provider.h
 *
 * Copyright 2018 Collabora Ltd.
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
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_TOOLCHAIN_PROVIDER (ide_toolchain_provider_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeToolchainProvider, ide_toolchain_provider, IDE, TOOLCHAIN_PROVIDER, IdeObject)

struct _IdeToolchainProviderInterface
{
  GTypeInterface parent_iface;

  void        (*load_async)       (IdeToolchainProvider  *self,
                                   GCancellable          *cancellable,
                                   GAsyncReadyCallback    callback,
                                   gpointer               user_data);
  gboolean    (*load_finish)      (IdeToolchainProvider  *self,
                                   GAsyncResult          *result,
                                   GError               **error);
  void        (*unload)           (IdeToolchainProvider  *self,
                                   IdeToolchainManager   *manager);
  void        (*added)            (IdeToolchainProvider  *self,
                                   IdeToolchain          *toolchain);
  void        (*removed)          (IdeToolchainProvider  *self,
                                   IdeToolchain          *toolchain);
};

IDE_AVAILABLE_IN_ALL
void     ide_toolchain_provider_load_async   (IdeToolchainProvider  *self,
                                              GCancellable          *cancellable,
                                              GAsyncReadyCallback    callback,
                                              gpointer               user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_toolchain_provider_load_finish  (IdeToolchainProvider  *self,
                                              GAsyncResult          *result,
                                              GError               **error);
IDE_AVAILABLE_IN_ALL
void     ide_toolchain_provider_unload       (IdeToolchainProvider  *self,
                                              IdeToolchainManager   *manager);
IDE_AVAILABLE_IN_ALL
void     ide_toolchain_provider_emit_added   (IdeToolchainProvider  *self,
                                              IdeToolchain          *toolchain);
IDE_AVAILABLE_IN_ALL
void     ide_toolchain_provider_emit_removed (IdeToolchainProvider  *self,
                                              IdeToolchain          *toolchain);

G_END_DECLS
