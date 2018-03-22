/* ide-settings.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SETTINGS (ide_settings_get_type())

G_DECLARE_FINAL_TYPE (IdeSettings, ide_settings, IDE, SETTINGS, IdeObject)

IdeSettings *_ide_settings_new                        (IdeContext              *context,
                                                       const gchar             *schema_id,
                                                       const gchar             *relative_path,
                                                       gboolean                 ignore_project_settings) G_GNUC_INTERNAL;
const gchar *ide_settings_get_relative_path           (IdeSettings             *self);
const gchar *ide_settings_get_schema_id               (IdeSettings             *self);
gboolean     ide_settings_get_ignore_project_settings (IdeSettings             *self);
GVariant    *ide_settings_get_default_value           (IdeSettings             *self,
                                                       const gchar             *key);
GVariant    *ide_settings_get_user_value              (IdeSettings             *self,
                                                       const gchar             *key);
GVariant    *ide_settings_get_value                   (IdeSettings             *self,
                                                       const gchar             *key);
void         ide_settings_set_value                   (IdeSettings             *self,
                                                       const gchar             *key,
                                                       GVariant                *value);
gboolean     ide_settings_get_boolean                 (IdeSettings             *self,
                                                       const gchar             *key);
gdouble      ide_settings_get_double                  (IdeSettings             *self,
                                                       const gchar             *key);
gint         ide_settings_get_int                     (IdeSettings             *self,
                                                       const gchar             *key);
gchar       *ide_settings_get_string                  (IdeSettings             *self,
                                                       const gchar             *key);
guint        ide_settings_get_uint                    (IdeSettings             *self,
                                                       const gchar             *key);
void         ide_settings_set_boolean                 (IdeSettings             *self,
                                                       const gchar             *key,
                                                       gboolean                 val);
void         ide_settings_set_double                  (IdeSettings             *self,
                                                       const gchar             *key,
                                                       gdouble                  val);
void         ide_settings_set_int                     (IdeSettings             *self,
                                                       const gchar             *key,
                                                       gint                     val);
void         ide_settings_set_string                  (IdeSettings             *self,
                                                       const gchar             *key,
                                                       const gchar             *val);
void         ide_settings_set_uint                    (IdeSettings             *self,
                                                       const gchar             *key,
                                                       guint                    val);
void         ide_settings_bind                        (IdeSettings             *self,
                                                       const gchar             *key,
                                                       gpointer                 object,
                                                       const gchar             *property,
                                                       GSettingsBindFlags       flags);
void         ide_settings_bind_with_mapping           (IdeSettings             *self,
                                                       const gchar             *key,
                                                       gpointer                 object,
                                                       const gchar             *property,
                                                       GSettingsBindFlags       flags,
                                                       GSettingsBindGetMapping  get_mapping,
                                                       GSettingsBindSetMapping  set_mapping,
                                                       gpointer                 user_data,
                                                       GDestroyNotify           destroy);
void         ide_settings_unbind                      (IdeSettings             *self,
                                                       const gchar             *property);

G_END_DECLS
