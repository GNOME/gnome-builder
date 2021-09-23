/* ide-settings-sandwich.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_SETTINGS_SANDWICH (ide_settings_sandwich_get_type())

G_DECLARE_FINAL_TYPE (IdeSettingsSandwich, ide_settings_sandwich, IDE, SETTINGS_SANDWICH, GObject)

IdeSettingsSandwich *ide_settings_sandwich_new               (const gchar             *schema_id,
                                                              const gchar             *path);
GVariant            *ide_settings_sandwich_get_default_value (IdeSettingsSandwich     *self,
                                                              const gchar             *key);
GVariant            *ide_settings_sandwich_get_user_value    (IdeSettingsSandwich     *self,
                                                              const gchar             *key);
GVariant            *ide_settings_sandwich_get_value         (IdeSettingsSandwich     *self,
                                                              const gchar             *key);
void                 ide_settings_sandwich_set_value         (IdeSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              GVariant                *value);
gboolean             ide_settings_sandwich_get_boolean       (IdeSettingsSandwich     *self,
                                                              const gchar             *key);
gdouble              ide_settings_sandwich_get_double        (IdeSettingsSandwich     *self,
                                                              const gchar             *key);
gint                 ide_settings_sandwich_get_int           (IdeSettingsSandwich     *self,
                                                              const gchar             *key);
gchar               *ide_settings_sandwich_get_string        (IdeSettingsSandwich     *self,
                                                              const gchar             *key);
guint                ide_settings_sandwich_get_uint          (IdeSettingsSandwich     *self,
                                                              const gchar             *key);
void                 ide_settings_sandwich_set_boolean       (IdeSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              gboolean                 val);
void                 ide_settings_sandwich_set_double        (IdeSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              gdouble                  val);
void                 ide_settings_sandwich_set_int           (IdeSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              gint                     val);
void                 ide_settings_sandwich_set_string        (IdeSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              const gchar             *val);
void                 ide_settings_sandwich_set_uint          (IdeSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              guint                    val);
void                 ide_settings_sandwich_append            (IdeSettingsSandwich     *self,
                                                              GSettings               *settings);
void                 ide_settings_sandwich_bind              (IdeSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              gpointer                 object,
                                                              const gchar             *property,
                                                              GSettingsBindFlags       flags);
void                 ide_settings_sandwich_bind_with_mapping (IdeSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              gpointer                 object,
                                                              const gchar             *property,
                                                              GSettingsBindFlags       flags,
                                                              GSettingsBindGetMapping  get_mapping,
                                                              GSettingsBindSetMapping  set_mapping,
                                                              gpointer                 user_data,
                                                              GDestroyNotify           destroy);
void                 ide_settings_sandwich_unbind            (IdeSettingsSandwich     *self,
                                                              const gchar             *property);

G_END_DECLS
