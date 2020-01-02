/* ide-application-shortcuts.c
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-application-shortcuts"

#include "config.h"

#include <glib/gi18n.h>
#include <dazzle.h>

#include "ide-application-private.h"

#define I_(s) (g_intern_static_string(s))

void
_ide_application_init_shortcuts (IdeApplication *self)
{
  DzlShortcutManager *manager;
  DzlShortcutTheme *theme;

  g_assert (IDE_IS_APPLICATION (self));

  manager = dzl_application_get_shortcut_manager (DZL_APPLICATION (self));
  theme = dzl_shortcut_manager_get_theme_by_name (manager, "internal");

  dzl_shortcut_manager_add_action (manager,
                                   I_("app.help"),
                                   N_("Workbench shortcuts"),
                                   N_("Help"),
                                   N_("Show the help window"),
                                   NULL);
  dzl_shortcut_theme_set_accel_for_action (theme,
                                           "app.help",
                                           "F1",
                                           DZL_SHORTCUT_PHASE_GLOBAL);

  dzl_shortcut_manager_add_action (manager,
                                   I_("app.preferences"),
                                   N_("Workbench shortcuts"),
                                   N_("Preferences"),
                                   N_("Show the preferences window"),
                                   NULL);
  dzl_shortcut_theme_set_accel_for_action (theme,
                                           "app.preferences",
                                           "<Primary>comma",
                                           DZL_SHORTCUT_PHASE_GLOBAL);

  dzl_shortcut_manager_add_action (manager,
                                   I_("app.shortcuts"),
                                   N_("Workbench shortcuts"),
                                   N_("Help"),
                                   N_("Show the shortcuts window"),
                                   NULL);
  dzl_shortcut_theme_set_accel_for_action (theme,
                                           "app.shortcuts",
                                           "<Primary>question",
                                           DZL_SHORTCUT_PHASE_GLOBAL);
}
