/* ide-settings-flag-action.h
 *
 * Copyright (C) 2015-2022 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_SETTINGS_FLAG_ACTION (ide_settings_flag_action_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSettingsFlagAction, ide_settings_flag_action, IDE, SETTINGS_FLAG_ACTION, GObject)

IDE_AVAILABLE_IN_ALL
IdeSettingsFlagAction *ide_settings_flag_action_new (const char *schema_id,
                                                     const char *schema_key,
                                                     const char *path,
                                                     const char *flag_nick);

G_END_DECLS
