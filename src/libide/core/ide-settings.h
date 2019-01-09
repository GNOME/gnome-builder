/* ide-settings.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_SETTINGS (ide_settings_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeSettings, ide_settings, IDE, SETTINGS, GObject)

IDE_AVAILABLE_IN_3_32
IdeSettings *ide_settings_new                         (const gchar             *project_id,
                                                       const gchar             *schema_id,
                                                       const gchar             *relative_path,
                                                       gboolean                 ignore_project_settings);
IDE_AVAILABLE_IN_3_32
const gchar *ide_settings_get_relative_path           (IdeSettings             *self);
IDE_AVAILABLE_IN_3_32
const gchar *ide_settings_get_schema_id               (IdeSettings             *self);
IDE_AVAILABLE_IN_3_32
gboolean     ide_settings_get_ignore_project_settings (IdeSettings             *self);
IDE_AVAILABLE_IN_3_32
GVariant    *ide_settings_get_default_value           (IdeSettings             *self,
                                                       const gchar             *key);
IDE_AVAILABLE_IN_3_32
GVariant    *ide_settings_get_user_value              (IdeSettings             *self,
                                                       const gchar             *key);
IDE_AVAILABLE_IN_3_32
GVariant    *ide_settings_get_value                   (IdeSettings             *self,
                                                       const gchar             *key);
IDE_AVAILABLE_IN_3_32
void         ide_settings_set_value                   (IdeSettings             *self,
                                                       const gchar             *key,
                                                       GVariant                *value);
IDE_AVAILABLE_IN_3_32
gboolean     ide_settings_get_boolean                 (IdeSettings             *self,
                                                       const gchar             *key);
IDE_AVAILABLE_IN_3_32
gdouble      ide_settings_get_double                  (IdeSettings             *self,
                                                       const gchar             *key);
IDE_AVAILABLE_IN_3_32
gint         ide_settings_get_int                     (IdeSettings             *self,
                                                       const gchar             *key);
IDE_AVAILABLE_IN_3_32
gchar       *ide_settings_get_string                  (IdeSettings             *self,
                                                       const gchar             *key);
IDE_AVAILABLE_IN_3_32
guint        ide_settings_get_uint                    (IdeSettings             *self,
                                                       const gchar             *key);
IDE_AVAILABLE_IN_3_32
void         ide_settings_set_boolean                 (IdeSettings             *self,
                                                       const gchar             *key,
                                                       gboolean                 val);
IDE_AVAILABLE_IN_3_32
void         ide_settings_set_double                  (IdeSettings             *self,
                                                       const gchar             *key,
                                                       gdouble                  val);
IDE_AVAILABLE_IN_3_32
void         ide_settings_set_int                     (IdeSettings             *self,
                                                       const gchar             *key,
                                                       gint                     val);
IDE_AVAILABLE_IN_3_32
void         ide_settings_set_string                  (IdeSettings             *self,
                                                       const gchar             *key,
                                                       const gchar             *val);
IDE_AVAILABLE_IN_3_32
void         ide_settings_set_uint                    (IdeSettings             *self,
                                                       const gchar             *key,
                                                       guint                    val);
IDE_AVAILABLE_IN_3_32
void         ide_settings_bind                        (IdeSettings             *self,
                                                       const gchar             *key,
                                                       gpointer                 object,
                                                       const gchar             *property,
                                                       GSettingsBindFlags       flags);
IDE_AVAILABLE_IN_3_32
void         ide_settings_bind_with_mapping           (IdeSettings             *self,
                                                       const gchar             *key,
                                                       gpointer                 object,
                                                       const gchar             *property,
                                                       GSettingsBindFlags       flags,
                                                       GSettingsBindGetMapping  get_mapping,
                                                       GSettingsBindSetMapping  set_mapping,
                                                       gpointer                 user_data,
                                                       GDestroyNotify           destroy);
IDE_AVAILABLE_IN_3_32
void         ide_settings_unbind                      (IdeSettings             *self,
                                                       const gchar             *property);

G_END_DECLS
