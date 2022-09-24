/* gbp-deviced-device.h
 *
 * Copyright 2018-2022 Christian Hergert <chergert@redhat.com>
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

#include <libide-foundry.h>
#include <libdeviced.h>

G_BEGIN_DECLS

#define GBP_TYPE_DEVICED_DEVICE (gbp_deviced_device_get_type())

G_DECLARE_FINAL_TYPE (GbpDevicedDevice, gbp_deviced_device, GBP, DEVICED_DEVICE, IdeDevice)

GbpDevicedDevice *gbp_deviced_device_new                   (DevdDevice             *device);
DevdDevice       *gbp_deviced_device_get_device            (GbpDevicedDevice       *self);
void              gbp_deviced_device_get_commit_async      (GbpDevicedDevice       *self,
                                                            const gchar            *commit_id,
                                                            GCancellable           *cancellable,
                                                            GAsyncReadyCallback     callback,
                                                            gpointer                user_data);
gchar            *gbp_deviced_device_get_commit_finish     (GbpDevicedDevice       *self,
                                                            GAsyncResult           *result,
                                                            GError                **error);
void              gbp_deviced_device_install_bundle_async  (GbpDevicedDevice       *self,
                                                            const gchar            *bundle_path,
                                                            GFileProgressCallback   progress,
                                                            gpointer                progress_data,
                                                            GDestroyNotify          progress_data_destroy,
                                                            GCancellable           *cancellable,
                                                            GAsyncReadyCallback     callback,
                                                            gpointer                user_data);
gboolean          gbp_deviced_device_install_bundle_finish (GbpDevicedDevice       *self,
                                                            GAsyncResult           *result,
                                                            GError                **error);
void              gbp_deviced_device_get_client_async      (GbpDevicedDevice        *self,
                                                            GCancellable            *cancellable,
                                                            GAsyncReadyCallback     callback,
                                                            gpointer                user_data);
DevdClient       *gbp_deviced_device_get_client_finish     (GbpDevicedDevice       *self,
                                                            GAsyncResult           *result,
                                                            GError                **error);

G_END_DECLS
