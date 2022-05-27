/* ide-device-provider.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define IDE_TYPE_DEVICE_PROVIDER (ide_device_provider_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDeviceProvider, ide_device_provider, IDE, DEVICE_PROVIDER, IdeObject)

struct _IdeDeviceProviderClass
{
  IdeObjectClass parent_class;

  void     (*device_added)   (IdeDeviceProvider    *self,
                              IdeDevice            *device);
  void     (*device_removed) (IdeDeviceProvider    *self,
                              IdeDevice            *device);
  void     (*load_async)     (IdeDeviceProvider    *self,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);
  gboolean (*load_finish)    (IdeDeviceProvider    *self,
                              GAsyncResult         *result,
                              GError              **error);
};

IDE_AVAILABLE_IN_ALL
void       ide_device_provider_emit_device_added   (IdeDeviceProvider    *self,
                                                    IdeDevice            *device);
IDE_AVAILABLE_IN_ALL
void       ide_device_provider_emit_device_removed (IdeDeviceProvider    *self,
                                                    IdeDevice            *device);
IDE_AVAILABLE_IN_ALL
void       ide_device_provider_load_async          (IdeDeviceProvider    *self,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean   ide_device_provider_load_finish         (IdeDeviceProvider    *self,
                                                    GAsyncResult         *result,
                                                    GError              **error);
IDE_AVAILABLE_IN_ALL
GPtrArray *ide_device_provider_get_devices         (IdeDeviceProvider    *self);

G_END_DECLS
