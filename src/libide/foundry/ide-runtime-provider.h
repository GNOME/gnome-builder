/* ide-runtime-provider.h
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUNTIME_PROVIDER (ide_runtime_provider_get_type ())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeRuntimeProvider, ide_runtime_provider, IDE, RUNTIME_PROVIDER, IdeObject)

struct _IdeRuntimeProviderInterface
{
  GTypeInterface parent;

  void        (*load)             (IdeRuntimeProvider   *self,
                                   IdeRuntimeManager    *manager);
  void        (*unload)           (IdeRuntimeProvider   *self,
                                   IdeRuntimeManager    *manager);
  gboolean    (*can_install)      (IdeRuntimeProvider   *self,
                                   const gchar          *runtime_id);
  void        (*install_async)    (IdeRuntimeProvider   *self,
                                   const gchar          *runtime_id,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data);
  gboolean    (*install_finish)   (IdeRuntimeProvider   *self,
                                   GAsyncResult         *result,
                                   GError              **error);
  void        (*bootstrap_async)  (IdeRuntimeProvider   *self,
                                   IdePipeline     *pipeline,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data);
  IdeRuntime *(*bootstrap_finish) (IdeRuntimeProvider   *self,
                                   GAsyncResult         *result,
                                   GError              **error);
};

IDE_AVAILABLE_IN_3_32
void        ide_runtime_provider_load             (IdeRuntimeProvider   *self,
                                                   IdeRuntimeManager    *manager);
IDE_AVAILABLE_IN_3_32
void        ide_runtime_provider_unload           (IdeRuntimeProvider   *self,
                                                   IdeRuntimeManager    *manager);
IDE_AVAILABLE_IN_3_32
gboolean    ide_runtime_provider_can_install      (IdeRuntimeProvider   *self,
                                                   const gchar          *runtime_id);
IDE_AVAILABLE_IN_3_32
void        ide_runtime_provider_install_async    (IdeRuntimeProvider   *self,
                                                   const gchar          *runtime_id,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean    ide_runtime_provider_install_finish   (IdeRuntimeProvider   *self,
                                                   GAsyncResult         *result,
                                                   GError              **error);
IDE_AVAILABLE_IN_3_32
void        ide_runtime_provider_bootstrap_async  (IdeRuntimeProvider   *self,
                                                   IdePipeline     *pipeline,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              user_data);
IDE_AVAILABLE_IN_3_32
IdeRuntime *ide_runtime_provider_bootstrap_finish (IdeRuntimeProvider   *self,
                                                   GAsyncResult         *result,
                                                   GError              **error);

G_END_DECLS
