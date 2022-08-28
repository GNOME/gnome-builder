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

#if !defined (IDE_CORE_INSIDE) && !defined (IDE_CORE_COMPILATION)
# error "Only <libide-core.h> can be included directly."
#endif

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_SETTINGS (ide_settings_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSettings, ide_settings, IDE, SETTINGS, GObject)

IDE_AVAILABLE_IN_ALL
char        *ide_settings_resolve_schema_path         (const char              *schema_id,
                                                       const char              *project_id,
                                                       const char              *path_suffix);
IDE_AVAILABLE_IN_ALL
IdeSettings *ide_settings_new                         (const char              *project_id,
                                                       const char              *schema_id);
IDE_AVAILABLE_IN_ALL
IdeSettings *ide_settings_new_with_path               (const char              *project_id,
                                                       const char              *schema_id,
                                                       const char              *path);
IDE_AVAILABLE_IN_ALL
IdeSettings *ide_settings_new_relocatable_with_suffix (const char              *project_id,
                                                       const char              *schema_id,
                                                       const char              *path_suffix);
IDE_AVAILABLE_IN_ALL
const char  *ide_settings_get_schema_id               (IdeSettings             *self);
IDE_AVAILABLE_IN_ALL
GVariant    *ide_settings_get_default_value           (IdeSettings             *self,
                                                       const char              *key);
IDE_AVAILABLE_IN_ALL
GVariant    *ide_settings_get_user_value              (IdeSettings             *self,
                                                       const char              *key);
IDE_AVAILABLE_IN_ALL
GVariant    *ide_settings_get_value                   (IdeSettings             *self,
                                                       const char              *key);
IDE_AVAILABLE_IN_ALL
void         ide_settings_set_value                   (IdeSettings             *self,
                                                       const char              *key,
                                                       GVariant                *value);
IDE_AVAILABLE_IN_ALL
gboolean     ide_settings_get_boolean                 (IdeSettings             *self,
                                                       const char              *key);
IDE_AVAILABLE_IN_ALL
double       ide_settings_get_double                  (IdeSettings             *self,
                                                       const char              *key);
IDE_AVAILABLE_IN_ALL
int          ide_settings_get_int                     (IdeSettings             *self,
                                                       const char              *key);
IDE_AVAILABLE_IN_ALL
char        *ide_settings_get_string                  (IdeSettings             *self,
                                                       const char              *key);
IDE_AVAILABLE_IN_ALL
guint        ide_settings_get_uint                    (IdeSettings             *self,
                                                       const char              *key);
IDE_AVAILABLE_IN_ALL
void         ide_settings_set_boolean                 (IdeSettings             *self,
                                                       const char              *key,
                                                       gboolean                 val);
IDE_AVAILABLE_IN_ALL
void         ide_settings_set_double                  (IdeSettings             *self,
                                                       const char              *key,
                                                       double                   val);
IDE_AVAILABLE_IN_ALL
void         ide_settings_set_int                     (IdeSettings             *self,
                                                       const char              *key,
                                                       int                      val);
IDE_AVAILABLE_IN_ALL
void         ide_settings_set_string                  (IdeSettings             *self,
                                                       const char              *key,
                                                       const char              *val);
IDE_AVAILABLE_IN_ALL
void         ide_settings_set_uint                    (IdeSettings             *self,
                                                       const char              *key,
                                                       guint                    val);
IDE_AVAILABLE_IN_ALL
void         ide_settings_bind                        (IdeSettings             *self,
                                                       const char              *key,
                                                       gpointer                 object,
                                                       const char              *property,
                                                       GSettingsBindFlags       flags);
IDE_AVAILABLE_IN_ALL
void         ide_settings_bind_with_mapping           (IdeSettings             *self,
                                                       const char              *key,
                                                       gpointer                 object,
                                                       const char              *property,
                                                       GSettingsBindFlags       flags,
                                                       GSettingsBindGetMapping  get_mapping,
                                                       GSettingsBindSetMapping  set_mapping,
                                                       gpointer                 user_data,
                                                       GDestroyNotify           destroy);
IDE_AVAILABLE_IN_ALL
void         ide_settings_unbind                      (IdeSettings             *self,
                                                       const char              *property);

G_END_DECLS
