/* ide-tweaks-settings.h
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

#include "ide-tweaks.h"
#include "ide-tweaks-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_SETTINGS (ide_tweaks_settings_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaksSettings, ide_tweaks_settings, IDE, TWEAKS_SETTINGS, IdeTweaksItem)

IDE_AVAILABLE_IN_ALL
IdeTweaksSettings *ide_tweaks_settings_new                  (void);
IDE_AVAILABLE_IN_ALL
const char        *ide_tweaks_settings_get_schema_id        (IdeTweaksSettings       *self);
IDE_AVAILABLE_IN_ALL
void               ide_tweaks_settings_set_schema_id        (IdeTweaksSettings       *self,
                                                             const char              *schema_id);
IDE_AVAILABLE_IN_ALL
const char        *ide_tweaks_settings_get_schema_path      (IdeTweaksSettings       *self);
IDE_AVAILABLE_IN_ALL
void               ide_tweaks_settings_set_schema_path      (IdeTweaksSettings       *self,
                                                             const char              *schema_path);
IDE_AVAILABLE_IN_ALL
gboolean           ide_tweaks_settings_get_application_only (IdeTweaksSettings       *self);
IDE_AVAILABLE_IN_ALL
void               ide_tweaks_settings_set_application_only (IdeTweaksSettings       *self,
                                                             gboolean                 application_only);
IDE_AVAILABLE_IN_ALL
GActionGroup      *ide_tweaks_settings_create_action_group  (IdeTweaksSettings       *self,
                                                             IdeTweaks               *tweaks);
IDE_AVAILABLE_IN_ALL
void               ide_tweaks_settings_bind                 (IdeTweaksSettings       *self,
                                                             const char              *key,
                                                             gpointer                 instance,
                                                             const char              *property,
                                                             GSettingsBindFlags       bind_flags);
IDE_AVAILABLE_IN_ALL
void               ide_tweaks_settings_bind_with_mapping    (IdeTweaksSettings       *self,
                                                             const char              *key,
                                                             gpointer                 instance,
                                                             const char              *property,
                                                             GSettingsBindFlags       bind_flags,
                                                             GSettingsBindGetMapping  get_mapping,
                                                             GSettingsBindSetMapping  set_mapping,
                                                             gpointer                 user_data,
                                                             GDestroyNotify           destroy);
IDE_AVAILABLE_IN_ALL
char              *ide_tweaks_settings_get_string           (IdeTweaksSettings       *self,
                                                             const char              *key);
IDE_AVAILABLE_IN_ALL
void               ide_tweaks_settings_set_string           (IdeTweaksSettings       *self,
                                                             const char              *key,
                                                             const char              *value);

G_END_DECLS
