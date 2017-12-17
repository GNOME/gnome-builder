/* ide-build-target-provider.h
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include "ide-object.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_TARGET_PROVIDER (ide_build_target_provider_get_type())

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

IDE_AVAILABLE_IN_3_28
void       ide_build_target_provider_get_targets_async  (IdeBuildTargetProvider  *self,
                                                         GCancellable            *cancellable,
                                                         GAsyncReadyCallback      callback,
                                                         gpointer                 user_data);
IDE_AVAILABLE_IN_3_28
GPtrArray *ide_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *self,
                                                         GAsyncResult            *result,
                                                         GError                 **error);

G_END_DECLS
