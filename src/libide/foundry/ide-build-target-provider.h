/* ide-build-target-provider.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_TARGET_PROVIDER (ide_build_target_provider_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeBuildTargetProvider, ide_build_target_provider, IDE, BUILD_TARGET_PROVIDER, IdeObject)

struct _IdeBuildTargetProviderInterface
{
  GTypeInterface parent_iface;

  void       (*get_targets_async)  (IdeBuildTargetProvider  *self,
                                    GCancellable            *cancellable,
                                    GAsyncReadyCallback      callback,
                                    gpointer                 user_data);
  GPtrArray *(*get_targets_finish) (IdeBuildTargetProvider  *self,
                                    GAsyncResult            *result,
                                    GError                 **error);
};

IDE_AVAILABLE_IN_ALL
void       ide_build_target_provider_get_targets_async  (IdeBuildTargetProvider  *self,
                                                         GCancellable            *cancellable,
                                                         GAsyncReadyCallback      callback,
                                                         gpointer                 user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray *ide_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *self,
                                                         GAsyncResult            *result,
                                                         GError                 **error);

G_END_DECLS
