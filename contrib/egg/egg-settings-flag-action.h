/* egg-settings-flag-action.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef EGG_SETTINGS_FLAG_ACTION_H
#define EGG_SETTINGS_FLAG_ACTION_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define EGG_TYPE_SETTINGS_FLAG_ACTION (egg_settings_flag_action_get_type())

G_DECLARE_FINAL_TYPE (EggSettingsFlagAction, egg_settings_flag_action, EGG, SETTINGS_FLAG_ACTION, GObject)

GAction *egg_settings_flag_action_new (const gchar *schema_id,
                                       const gchar *schema_key,
                                       const gchar *flag_nick);

G_END_DECLS

#endif /* EGG_SETTINGS_FLAG_ACTION_H */
