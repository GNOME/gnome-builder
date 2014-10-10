/* gb-command-vim.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-command-vim.h"
#include "gb-editor-tab-private.h"

static void
gb_command_vim_activate (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  GbEditorTab *tab;

  g_return_if_fail (G_IS_SIMPLE_ACTION (action));
  g_return_if_fail (parameter);

  tab = g_object_get_data (G_OBJECT (action), "GB_EDITOR_TAB");

  if (!GB_IS_EDITOR_TAB (tab))
    {
      g_warning ("Failed to retrieve editor tab!");
      return;
    }

  if (g_variant_is_of_type (parameter, G_VARIANT_TYPE_STRING))
    {
      const gchar *command_text;

      command_text = g_variant_get_string (parameter, NULL);
      gb_editor_vim_execute_command (tab->priv->vim, command_text);
    }
}

GAction *
gb_command_vim_new (GbEditorTab *tab)
{
  GSimpleAction *action;

  g_return_val_if_fail (GB_IS_EDITOR_TAB (tab), NULL);

  action = g_simple_action_new ("vim-command", G_VARIANT_TYPE_STRING);
  g_object_set_data_full (G_OBJECT (action), "GB_EDITOR_TAB",
                          g_object_ref (tab), g_object_unref);
  g_signal_connect (action, "activate", G_CALLBACK (gb_command_vim_activate),
                    NULL);

  return G_ACTION (action);
}
