/* ide-sdk.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_SDK (ide_sdk_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeSdk, ide_sdk, IDE, SDK, GObject)

struct _IdeSdkClass
{
  GObjectClass parent_class;
};

IDE_AVAILABLE_IN_ALL
IdeSdkProvider *ide_sdk_get_provider   (IdeSdk     *self);
IDE_AVAILABLE_IN_ALL
gboolean        ide_sdk_get_can_update (IdeSdk     *self);
IDE_AVAILABLE_IN_ALL
void            ide_sdk_set_can_update (IdeSdk     *self,
                                        gboolean    can_update);
IDE_AVAILABLE_IN_ALL
gboolean        ide_sdk_get_installed  (IdeSdk     *self);
IDE_AVAILABLE_IN_ALL
void            ide_sdk_set_installed  (IdeSdk     *self,
                                        gboolean    installed);
IDE_AVAILABLE_IN_ALL
const char     *ide_sdk_get_title      (IdeSdk     *self);
IDE_AVAILABLE_IN_ALL
void            ide_sdk_set_title      (IdeSdk     *self,
                                        const char *title);
IDE_AVAILABLE_IN_ALL
const char     *ide_sdk_get_subtitle   (IdeSdk     *self);
IDE_AVAILABLE_IN_ALL
void            ide_sdk_set_subtitle   (IdeSdk     *self,
                                        const char *subtitle);

G_END_DECLS
