/* egg-settings-sandwich.h
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

#ifndef EGG_SETTINGS_SANDWICH_H
#define EGG_SETTINGS_SANDWICH_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define EGG_TYPE_SETTINGS_SANDWICH (egg_settings_sandwich_get_type())

G_DECLARE_FINAL_TYPE (EggSettingsSandwich, egg_settings_sandwich, EGG, SETTINGS_SANDWICH, GObject)

EggSettingsSandwich *egg_settings_sandwich_new               (const gchar             *schema_id,
                                                              const gchar             *path);
GVariant            *egg_settings_sandwich_get_default_value (EggSettingsSandwich     *self,
                                                              const gchar             *key);
GVariant            *egg_settings_sandwich_get_user_value    (EggSettingsSandwich     *self,
                                                              const gchar             *key);
GVariant            *egg_settings_sandwich_get_value         (EggSettingsSandwich     *self,
                                                              const gchar             *key);
void                 egg_settings_sandwich_set_value         (EggSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              GVariant                *value);
gboolean             egg_settings_sandwich_get_boolean       (EggSettingsSandwich     *self,
                                                              const gchar             *key);
gdouble              egg_settings_sandwich_get_double        (EggSettingsSandwich     *self,
                                                              const gchar             *key);
gint                 egg_settings_sandwich_get_int           (EggSettingsSandwich     *self,
                                                              const gchar             *key);
gchar               *egg_settings_sandwich_get_string        (EggSettingsSandwich     *self,
                                                              const gchar             *key);
guint                egg_settings_sandwich_get_uint          (EggSettingsSandwich     *self,
                                                              const gchar             *key);
void                 egg_settings_sandwich_set_boolean       (EggSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              gboolean                 val);
void                 egg_settings_sandwich_set_double        (EggSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              gdouble                  val);
void                 egg_settings_sandwich_set_int           (EggSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              gint                     val);
void                 egg_settings_sandwich_set_string        (EggSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              const gchar             *val);
void                 egg_settings_sandwich_set_uint          (EggSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              guint                    val);
void                 egg_settings_sandwich_append            (EggSettingsSandwich     *self,
                                                              GSettings               *settings);
void                 egg_settings_sandwich_bind              (EggSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              gpointer                 object,
                                                              const gchar             *property,
                                                              GSettingsBindFlags       flags);
void                 egg_settings_sandwich_bind_with_mapping (EggSettingsSandwich     *self,
                                                              const gchar             *key,
                                                              gpointer                 object,
                                                              const gchar             *property,
                                                              GSettingsBindFlags       flags,
                                                              GSettingsBindGetMapping  get_mapping,
                                                              GSettingsBindSetMapping  set_mapping,
                                                              gpointer                 user_data,
                                                              GDestroyNotify           destroy);
void                 egg_settings_sandwich_unbind            (EggSettingsSandwich     *self,
                                                              const gchar             *property);

G_END_DECLS

#endif /* EGG_SETTINGS_SANDWICH_H */
