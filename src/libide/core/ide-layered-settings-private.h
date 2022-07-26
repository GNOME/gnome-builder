/* ide-layered-settings-private.h
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

#define IDE_TYPE_LAYERED_SETTINGS (ide_layered_settings_get_type())

G_DECLARE_FINAL_TYPE (IdeLayeredSettings, ide_layered_settings, IDE, LAYERED_SETTINGS, GObject)

IdeLayeredSettings  *ide_layered_settings_new               (const char              *schema_id,
                                                             const char              *path);
GSettingsSchemaKey  *ide_layered_settings_get_key           (IdeLayeredSettings      *self,
                                                             const char              *key);
char               **ide_layered_settings_list_keys         (IdeLayeredSettings      *self);
GVariant            *ide_layered_settings_get_default_value (IdeLayeredSettings      *self,
                                                             const char              *key);
GVariant            *ide_layered_settings_get_user_value    (IdeLayeredSettings      *self,
                                                             const char              *key);
GVariant            *ide_layered_settings_get_value         (IdeLayeredSettings      *self,
                                                             const char              *key);
void                 ide_layered_settings_set_value         (IdeLayeredSettings      *self,
                                                             const char              *key,
                                                             GVariant                *value);
gboolean             ide_layered_settings_get_boolean       (IdeLayeredSettings      *self,
                                                             const char              *key);
double               ide_layered_settings_get_double        (IdeLayeredSettings      *self,
                                                             const char              *key);
int                  ide_layered_settings_get_int           (IdeLayeredSettings      *self,
                                                             const char              *key);
char                *ide_layered_settings_get_string        (IdeLayeredSettings      *self,
                                                             const char              *key);
guint                ide_layered_settings_get_uint          (IdeLayeredSettings      *self,
                                                             const char              *key);
void                 ide_layered_settings_set_boolean       (IdeLayeredSettings      *self,
                                                             const char              *key,
                                                             gboolean                 val);
void                 ide_layered_settings_set_double        (IdeLayeredSettings      *self,
                                                             const char              *key,
                                                             double                   val);
void                 ide_layered_settings_set_int           (IdeLayeredSettings      *self,
                                                             const char              *key,
                                                             int                      val);
void                 ide_layered_settings_set_string        (IdeLayeredSettings      *self,
                                                             const char              *key,
                                                             const char              *val);
void                 ide_layered_settings_set_uint          (IdeLayeredSettings      *self,
                                                             const char              *key,
                                                             guint                    val);
void                 ide_layered_settings_append            (IdeLayeredSettings      *self,
                                                             GSettings               *settings);
void                 ide_layered_settings_bind              (IdeLayeredSettings      *self,
                                                             const char              *key,
                                                             gpointer                 object,
                                                             const char              *property,
                                                             GSettingsBindFlags       flags);
void                 ide_layered_settings_bind_with_mapping (IdeLayeredSettings      *self,
                                                             const char              *key,
                                                             gpointer                 object,
                                                             const char              *property,
                                                             GSettingsBindFlags       flags,
                                                             GSettingsBindGetMapping  get_mapping,
                                                             GSettingsBindSetMapping  set_mapping,
                                                             gpointer                 user_data,
                                                             GDestroyNotify           destroy);
void                 ide_layered_settings_unbind            (IdeLayeredSettings      *self,
                                                             const char              *property);

G_END_DECLS
