/* gb-workbench-actions.c
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

#define G_LOG_DOMAIN "gb-workbench-actions"

#include "gb-workbench.h"
#include "gb-workbench-actions.h"
#include "gb-workbench-private.h"

static void
gb_workbench_actions_build (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
}

static void
gb_workbench_actions_global_search (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GbWorkbench *self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->search_box));
}

static void
gb_workbench_actions_open_uri_list (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GbWorkbench *self = user_data;
  const gchar **uri_list;

  g_assert (GB_IS_WORKBENCH (self));

  uri_list = g_variant_get_strv (parameter, NULL);

  if (uri_list != NULL)
    {
      gsize i;

      for (i = 0; uri_list [i]; i++)
        {
          g_autoptr(GFile) file = NULL;

          file = g_file_new_for_uri (uri_list [i]);
          gb_workbench_open (self, file);
        }

      g_free (uri_list);
    }
}

static void
gb_workbench_actions_save_all (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
}

static void
gb_workbench_actions_show_command_bar (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
  GbWorkbench *self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gb_command_bar_show (self->command_bar);
}

static const GActionEntry GbWorkbenchActions[] = {
  { "build",            gb_workbench_actions_build },
  { "global-search",    gb_workbench_actions_global_search },
  { "open-uri-list",    gb_workbench_actions_open_uri_list, "as" },
  { "save-all",         gb_workbench_actions_save_all },
  { "show-command-bar", gb_workbench_actions_show_command_bar },
};

void
gb_workbench_actions_init (GbWorkbench *self)
{
  GSimpleActionGroup *actions;

  g_assert (GB_IS_WORKBENCH (self));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), GbWorkbenchActions,
                                   G_N_ELEMENTS (GbWorkbenchActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "workbench", G_ACTION_GROUP (actions));
}
