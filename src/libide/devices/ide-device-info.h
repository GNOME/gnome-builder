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

#include "ide-types.h"
#include "ide-version-macros.h"
#include "util/ide-triplet.h"

G_BEGIN_DECLS

typedef enum
{
  IDE_DEVICE_KIND_COMPUTER,
  IDE_DEVICE_KIND_PHONE,
  IDE_DEVICE_KIND_TABLET,
  IDE_DEVICE_KIND_MICRO_CONTROLLER,
} IdeDeviceKind;

#define IDE_TYPE_DEVICE_INFO (ide_device_info_get_type())

IDE_AVAILABLE_IN_3_28
G_DECLARE_FINAL_TYPE (IdeDeviceInfo, ide_device_info, IDE, DEVICE_INFO, GObject)

IDE_AVAILABLE_IN_3_28
IdeDeviceInfo *ide_device_info_new        (void);
IDE_AVAILABLE_IN_3_28
IdeDeviceKind ide_device_info_get_kind    (IdeDeviceInfo *self);
IDE_AVAILABLE_IN_3_28
void          ide_device_info_set_kind    (IdeDeviceInfo *self,
                                           IdeDeviceKind  kind);
IDE_AVAILABLE_IN_3_30
IdeTriplet   *ide_device_info_get_triplet (IdeDeviceInfo *self);
IDE_AVAILABLE_IN_3_30
void          ide_device_info_set_triplet (IdeDeviceInfo *self,
                                           IdeTriplet    *triplet);

G_END_DECLS
