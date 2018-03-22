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
 */

#define G_LOG_DOMAIN "ide-application-shortcuts"

#include "config.h"

#include <glib/gi18n.h>
#include <dazzle.h>

#include "application/ide-application-private.h"

#define I_(s) (g_intern_static_string(s))

void
_ide_application_init_shortcuts (IdeApplication *self)
{
  static const gchar *shortcuts_fallback[] = { "<Primary>F1", NULL };
  DzlShortcutManager *manager;
  DzlShortcutTheme *theme;

  g_assert (IDE_IS_APPLICATION (self));

  manager = dzl_application_get_shortcut_manager (DZL_APPLICATION (self));
  theme = g_object_ref (dzl_shortcut_manager_get_theme (manager));

  dzl_shortcut_manager_add_action (manager,
                                   I_("app.help"),
                                   NC_("shortcut window", "Workbench shortcuts"),
                                   NC_("shortcut window", "Help"),
                                   NC_("shortcut window", "Show the help window"),
                                   NULL);
  dzl_shortcut_theme_set_accel_for_action (theme,
                                           "app.help",
                                           "F1",
                                           DZL_SHORTCUT_PHASE_GLOBAL);

  dzl_shortcut_manager_add_action (manager,
                                   I_("app.preferences"),
                                   NC_("shortcut window", "Workbench shortcuts"),
                                   NC_("shortcut window", "Preferences"),
                                   NC_("shortcut window", "Show the preferences window"),
                                   NULL);
  dzl_shortcut_theme_set_accel_for_action (theme,
                                           "app.preferences",
                                           "<Primary>comma",
                                           DZL_SHORTCUT_PHASE_GLOBAL);

  dzl_shortcut_manager_add_action (manager,
                                   I_("app.shortcuts"),
                                   NC_("shortcut window", "Workbench shortcuts"),
                                   NC_("shortcut window", "Help"),
                                   NC_("shortcut window", "Show the shortcuts window"),
                                   NULL);
  dzl_shortcut_theme_set_accel_for_action (theme,
                                           "app.shortcuts",
                                           "<Primary><Shift>question",
                                           DZL_SHORTCUT_PHASE_GLOBAL);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "app.shortcuts",
                                         shortcuts_fallback);
}
