/* gbp-deviced-device.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <ide.h>
#include <libdeviced.h>

G_BEGIN_DECLS

#define GBP_TYPE_DEVICED_DEVICE (gbp_deviced_device_get_type())

G_DECLARE_FINAL_TYPE (GbpDevicedDevice, gbp_deviced_device, GBP, DEVICED_DEVICE, IdeDevice)

GbpDevicedDevice *gbp_deviced_device_new (IdeContext *context,
                                          DevdDevice *device);

G_END_DECLS
