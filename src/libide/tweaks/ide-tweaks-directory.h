/* ide-tweaks-directory.h
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

#if !defined (IDE_TWEAKS_INSIDE) && !defined (IDE_TWEAKS_COMPILATION)
# error "Only <libide-tweaks.h> can be included directly."
#endif

#include "ide-tweaks-settings.h"
#include "ide-tweaks-widget.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_DIRECTORY (ide_tweaks_directory_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaksDirectory, ide_tweaks_directory, IDE, TWEAKS_DIRECTORY, IdeTweaksWidget)

IDE_AVAILABLE_IN_ALL
IdeTweaksDirectory *ide_tweaks_directory_new              (void);
IDE_AVAILABLE_IN_ALL
const char         *ide_tweaks_directory_get_title        (IdeTweaksDirectory *self);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_directory_set_title        (IdeTweaksDirectory *self,
                                                           const char         *title);
IDE_AVAILABLE_IN_ALL
const char         *ide_tweaks_directory_get_subtitle     (IdeTweaksDirectory *self);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_directory_set_subtitle     (IdeTweaksDirectory *self,
                                                           const char         *subtitle);
IDE_AVAILABLE_IN_ALL
GObject            *ide_tweaks_directory_get_object       (IdeTweaksDirectory *self);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_directory_set_object       (IdeTweaksDirectory *self,
                                                           GObject            *object);
IDE_AVAILABLE_IN_ALL
const char         *ide_tweaks_directory_get_key          (IdeTweaksDirectory *self);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_directory_set_key          (IdeTweaksDirectory *self,
                                                           const char         *key);
IDE_AVAILABLE_IN_ALL
IdeTweaksSettings  *ide_tweaks_directory_get_settings     (IdeTweaksDirectory *self);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_directory_set_settings     (IdeTweaksDirectory *self,
                                                           IdeTweaksSettings  *settings);
IDE_AVAILABLE_IN_ALL
gboolean            ide_tweaks_directory_get_is_directory (IdeTweaksDirectory *self);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_directory_set_is_directory (IdeTweaksDirectory *self,
                                                           gboolean            is_directory);

G_END_DECLS
