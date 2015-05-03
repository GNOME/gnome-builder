/* ide-settings.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_SETTINGS_H
#define IDE_SETTINGS_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SETTINGS (ide_settings_get_type())

G_DECLARE_FINAL_TYPE (IdeSettings, ide_settings, IDE, SETTINGS, IdeObject)

const gchar *ide_settings_get_relative_path     (IdeSettings *self);
const gchar *ide_settings_get_schema_id         (IdeSettings *self);
GVariant    *ide_settings_get_value             (IdeSettings *self,
                                                 const gchar *key);
GVariant    *ide_settings_get_default_value     (IdeSettings *self,
                                                 const gchar *key);
GVariant    *ide_settings_get_user_value        (IdeSettings *self,
                                                 const gchar *key);
guint        ide_settings_get_uint              (IdeSettings *self,
                                                 const gchar *key);
gint         ide_settings_get_int               (IdeSettings *self,
                                                 const gchar *key);
gboolean     ide_settings_get_gboolean          (IdeSettings *self,
                                                 const gchar *key);
gchar       *ide_settings_get_string            (IdeSettings *self,
                                                 const gchar *key);
gdouble      ide_settings_get_double            (IdeSettings *self,
                                                 const gchar *key);
gboolean     ide_settings_get_is_global         (IdeSettings *self);
void         ide_settings_set_is_global         (IdeSettings *self,
                                                 gboolean     is_global);

G_END_DECLS

#endif /* IDE_SETTINGS_H */
