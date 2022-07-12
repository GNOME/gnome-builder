/* ide-device-manager.h
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

#define IDE_TYPE_DEVICE_MANAGER (ide_device_manager_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeDeviceManager, ide_device_manager, IDE, DEVICE_MANAGER, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeDeviceManager *ide_device_manager_from_context         (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
gdouble           ide_device_manager_get_progress         (IdeDeviceManager     *self);
IDE_AVAILABLE_IN_ALL
IdeDevice        *ide_device_manager_get_device           (IdeDeviceManager     *self);
IDE_AVAILABLE_IN_ALL
void              ide_device_manager_set_device           (IdeDeviceManager     *self,
                                                           IdeDevice            *device);
IDE_AVAILABLE_IN_ALL
IdeDevice        *ide_device_manager_get_device_by_id     (IdeDeviceManager     *self,
                                                           const gchar          *device_id);
IDE_AVAILABLE_IN_ALL
void              ide_device_manager_deploy_async         (IdeDeviceManager     *self,
                                                           IdePipeline          *pipeline,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean          ide_device_manager_deploy_finish        (IdeDeviceManager     *self,
                                                           GAsyncResult         *result,
                                                           GError              **error);

G_END_DECLS
