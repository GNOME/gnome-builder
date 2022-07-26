/* ide-settings-sandwich-private.h
 *
 * Copyright 2015-2022 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_SETTINGS_SANDWICH (ide_settings_sandwich_get_type())

G_DECLARE_FINAL_TYPE (IdeSettingsSandwich, ide_settings_sandwich, IDE, SETTINGS_SANDWICH, GObject)

IdeSettingsSandwich *ide_settings_sandwich_new               (const char              *schema_id,
                                                              const char              *path);
GVariant            *ide_settings_sandwich_get_default_value (IdeSettingsSandwich     *self,
                                                              const char              *key);
GVariant            *ide_settings_sandwich_get_user_value    (IdeSettingsSandwich     *self,
                                                              const char              *key);
GVariant            *ide_settings_sandwich_get_value         (IdeSettingsSandwich     *self,
                                                              const char              *key);
void                 ide_settings_sandwich_set_value         (IdeSettingsSandwich     *self,
                                                              const char              *key,
                                                              GVariant                *value);
gboolean             ide_settings_sandwich_get_boolean       (IdeSettingsSandwich     *self,
                                                              const char              *key);
double               ide_settings_sandwich_get_double        (IdeSettingsSandwich     *self,
                                                              const char              *key);
int                  ide_settings_sandwich_get_int           (IdeSettingsSandwich     *self,
                                                              const char              *key);
char                *ide_settings_sandwich_get_string        (IdeSettingsSandwich     *self,
                                                              const char              *key);
guint                ide_settings_sandwich_get_uint          (IdeSettingsSandwich     *self,
                                                              const char              *key);
void                 ide_settings_sandwich_set_boolean       (IdeSettingsSandwich     *self,
                                                              const char              *key,
                                                              gboolean                 val);
void                 ide_settings_sandwich_set_double        (IdeSettingsSandwich     *self,
                                                              const char              *key,
                                                              double                   val);
void                 ide_settings_sandwich_set_int           (IdeSettingsSandwich     *self,
                                                              const char              *key,
                                                              int                      val);
void                 ide_settings_sandwich_set_string        (IdeSettingsSandwich     *self,
                                                              const char              *key,
                                                              const char              *val);
void                 ide_settings_sandwich_set_uint          (IdeSettingsSandwich     *self,
                                                              const char              *key,
                                                              guint                    val);
void                 ide_settings_sandwich_append            (IdeSettingsSandwich     *self,
                                                              GSettings               *settings);
void                 ide_settings_sandwich_bind              (IdeSettingsSandwich     *self,
                                                              const char              *key,
                                                              gpointer                 object,
                                                              const char              *property,
                                                              GSettingsBindFlags       flags);
void                 ide_settings_sandwich_bind_with_mapping (IdeSettingsSandwich     *self,
                                                              const char              *key,
                                                              gpointer                 object,
                                                              const char              *property,
                                                              GSettingsBindFlags       flags,
                                                              GSettingsBindGetMapping  get_mapping,
                                                              GSettingsBindSetMapping  set_mapping,
                                                              gpointer                 user_data,
                                                              GDestroyNotify           destroy);
void                 ide_settings_sandwich_unbind            (IdeSettingsSandwich     *self,
                                                              const char              *property);

G_END_DECLS
