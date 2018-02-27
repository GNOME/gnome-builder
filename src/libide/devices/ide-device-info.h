/* ide-device-info.h
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum
{
  IDE_DEVICE_KIND_COMPUTER,
  IDE_DEVICE_KIND_PHONE,
  IDE_DEVICE_KIND_TABLET,
  IDE_DEVICE_KIND_MICRO_CONTROLLER,
} IdeDeviceKind;

#define IDE_TYPE_DEVICE_INFO (ide_device_info_get_type())

G_DECLARE_FINAL_TYPE (IdeDeviceInfo, ide_device_info, IDE, DEVICE_INFO, GObject)

IdeDeviceInfo *ide_device_info_new        (void);
IdeDeviceKind  ide_device_info_get_kind   (IdeDeviceInfo *self);
void           ide_device_info_set_kind   (IdeDeviceInfo *self,
                                           IdeDeviceKind  kind);
const gchar   *ide_device_info_get_kernel (IdeDeviceInfo *self);
void           ide_device_info_set_kernel (IdeDeviceInfo *self,
                                           const gchar   *kernel);
const gchar   *ide_device_info_get_arch   (IdeDeviceInfo *self);
void           ide_device_info_set_arch   (IdeDeviceInfo *self,
                                           const gchar   *arch);
const gchar   *ide_device_info_get_system (IdeDeviceInfo *self);
void           ide_device_info_set_system (IdeDeviceInfo *self,
                                           const gchar   *system);

G_END_DECLS
