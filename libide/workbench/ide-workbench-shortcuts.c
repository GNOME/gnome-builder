/* ide-workbench-shortcuts.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-workbench-shortcuts"

#include "config.h"

#include <glib/gi18n.h>
#include <dazzle.h>

#include "ide-workbench-private.h"

void
_ide_workbench_add_perspective_shortcut (IdeWorkbench   *self,
                                         IdePerspective *perspective)
{
  g_autofree gchar *accel= NULL;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_PERSPECTIVE (perspective));

  accel = ide_perspective_get_accelerator (perspective);
  if (accel != NULL)
    {
      DzlShortcutController *controller;
      g_autofree gchar *id = ide_perspective_get_id (perspective);
      g_autofree gchar *title = ide_perspective_get_title (perspective);
      g_autofree gchar *command_id = g_strdup_printf ("org.gnome.builder.workbench.perspective('%s')", id);
      g_autofree gchar *action_name = g_strdup_printf ("win.perspective('%s')", id);
      g_autofree gchar *shortcut_help = g_strdup_printf ("Switch to %s perspective", title);
      const DzlShortcutEntry workbench_shortcut_entry[] = {
        { command_id,
          0, NULL,
          N_("Workbench shortcuts"),
          N_("Perspectives"),
          N_(shortcut_help) },
      };

      controller = dzl_shortcut_controller_find (GTK_WIDGET (self));
      dzl_shortcut_controller_add_command_action (controller,
                                                  command_id,
                                                  accel,
                                                  DZL_SHORTCUT_PHASE_GLOBAL,
                                                  action_name);

      dzl_shortcut_manager_add_shortcut_entries (NULL,
                                                 workbench_shortcut_entry,
                                                 G_N_ELEMENTS (workbench_shortcut_entry),
                                                 GETTEXT_PACKAGE);
    }
}
