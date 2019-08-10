/* gbp-shellcmd-preferences-addin.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-shellcmd-preferences-addin"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libide-gui.h>

#include "gbp-shellcmd-application-addin.h"
#include "gbp-shellcmd-command-editor.h"
#include "gbp-shellcmd-command-model.h"
#include "gbp-shellcmd-command-row.h"
#include "gbp-shellcmd-list.h"
#include "gbp-shellcmd-preferences-addin.h"

struct _GbpShellcmdPreferencesAddin
{
  GObject parent_instance;

  GbpShellcmdCommandEditor *editor;
};

static GbpShellcmdCommandModel *
get_model (void)
{
  GbpShellcmdApplicationAddin *app_addin;
  GbpShellcmdCommandModel *model;

  app_addin = ide_application_find_addin_by_module_name (NULL, "shellcmd");
  g_assert (GBP_IS_SHELLCMD_APPLICATION_ADDIN (app_addin));

  model = gbp_shellcmd_application_addin_get_model (app_addin);
  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (model));

  return model;
}

static void
on_command_selected_cb (GbpShellcmdPreferencesAddin *self,
                        GbpShellcmdCommand          *command,
                        GbpShellcmdList             *list)
{
  GtkWidget *preferences;

  g_assert (GBP_IS_SHELLCMD_PREFERENCES_ADDIN (self));
  g_assert (!command || GBP_IS_SHELLCMD_COMMAND (command));
  g_assert (GBP_IS_SHELLCMD_LIST (list));

  if (!(preferences = gtk_widget_get_ancestor (GTK_WIDGET (list), DZL_TYPE_PREFERENCES)))
    return;

  if (command != NULL)
    {
      g_autoptr(GHashTable) map = NULL;

      map = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
      g_hash_table_insert (map, (gchar *)"{id}", g_strdup (gbp_shellcmd_command_get_id (command)));
      dzl_preferences_set_page (DZL_PREFERENCES (preferences), "shellcmd.id", map);
    }

  gbp_shellcmd_command_editor_set_command (self->editor, command);
}

static void
gbp_shellcmd_preferences_addin_load (IdePreferencesAddin *addin,
                                     DzlPreferences      *prefs)
{
  GbpShellcmdPreferencesAddin *self = (GbpShellcmdPreferencesAddin *)addin;
  GtkWidget *list;

  g_assert (GBP_IS_SHELLCMD_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (prefs));

  dzl_preferences_add_page (prefs, "shellcmd", _("External Commands"), 650);
  dzl_preferences_add_group (prefs, "shellcmd", "commands", _("External Commands"), 0);

  list = gbp_shellcmd_list_new (get_model ());
  g_signal_connect_object (list,
                           "command-selected",
                           G_CALLBACK (on_command_selected_cb),
                           self,
                           G_CONNECT_SWAPPED);
  dzl_preferences_add_custom (prefs, "shellcmd", "commands", list, NULL, 0);

  dzl_preferences_add_page (prefs, "shellcmd.id", NULL, 0);
  dzl_preferences_add_group (prefs, "shellcmd.id", "basic", _("Command"), 0);

  self->editor = g_object_new (GBP_TYPE_SHELLCMD_COMMAND_EDITOR,
                               "visible", TRUE,
                               NULL);
  g_signal_connect (self->editor,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->editor);
  dzl_preferences_add_custom (prefs, "shellcmd.id", "basic", GTK_WIDGET (self->editor), NULL, 0);
}

static void
gbp_shellcmd_preferences_addin_unload (IdePreferencesAddin *addin,
                                       DzlPreferences      *prefs)
{
  GbpShellcmdPreferencesAddin *self = (GbpShellcmdPreferencesAddin *)addin;

  g_assert (GBP_IS_SHELLCMD_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (prefs));

  if (self->editor != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->editor));

  g_assert (self->editor == NULL);
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_shellcmd_preferences_addin_load;
  iface->unload = gbp_shellcmd_preferences_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpShellcmdPreferencesAddin, gbp_shellcmd_preferences_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_shellcmd_preferences_addin_class_init (GbpShellcmdPreferencesAddinClass *klass)
{
}

static void
gbp_shellcmd_preferences_addin_init (GbpShellcmdPreferencesAddin *self)
{
}
