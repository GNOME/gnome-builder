/* gb-command-gaction-provider.c
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

#define G_LOG_DOMAIN "gaction-commands"

#include <string.h>

#include "gb-command-gaction-provider.h"
#include "gb-log.h"

G_DEFINE_TYPE (GbCommandGactionProvider, gb_command_gaction_provider,
               GB_TYPE_COMMAND_PROVIDER)

GbCommandProvider *
gb_command_gaction_provider_new (GbWorkbench *workbench)
{
  return g_object_new (GB_TYPE_COMMAND_GACTION_PROVIDER,
                       "workbench", workbench,
                       NULL);
}

static GbCommandResult *
execute_action (GbCommand *command,
                gpointer   user_data)
{
  GAction *action;
  GVariant *params;

  g_return_val_if_fail (GB_IS_COMMAND (command), NULL);

  action = g_object_get_data (G_OBJECT (command), "action");
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  params = g_object_get_data (G_OBJECT (command), "parameters");

  g_action_activate (action, params);

  return NULL;
}

static GbCommand *
gb_command_gaction_provider_lookup (GbCommandProvider *provider,
                                    const gchar       *command_text)
{
  GbCommandGactionProvider *self = (GbCommandGactionProvider *)provider;
  GtkWidget *widget;
  GbCommand *command = NULL;
  GVariant *parameters = NULL;
  GAction *action = NULL;
  gchar **parts;
  gchar *tmp;
  gchar *name = NULL;

  ENTRY;

  g_return_val_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (self), NULL);

  /* Determine the command name */
  tmp = g_strdelimit (g_strdup (command_text), "(", ' ');
  parts = g_strsplit (tmp, " ", 2);
  name = g_strdup (parts [0]);
  g_free (tmp);
  g_strfreev (parts);

  /* Parse the parameters if provided */
  command_text += strlen (name);
  for (; *command_text; command_text = g_utf8_next_char (command_text))
    {
      gunichar ch;

      ch = g_utf8_get_char (command_text);
      if (g_unichar_isspace (ch))
        continue;
      break;
    }

  if (*command_text)
    {
      parameters = g_variant_parse (NULL, command_text, NULL, NULL, NULL);
      if (!parameters)
        GOTO (cleanup);
    }

  /*
   * TODO: We are missing some API in Gtk+ to be able to resolve an action
   *       for a particular name. It exists, but is currently only private
   *       API. So we'll hold off a bit on this until we can use that.
   *       Alternatively, we can just walk up the chain to the
   *       ApplicationWindow which is a GActionMap.
   */

  widget = GTK_WIDGET (gb_command_provider_get_active_tab (provider));
  while (widget)
    {
      if (G_IS_ACTION_MAP (widget))
        {
          action = g_action_map_lookup_action (G_ACTION_MAP (widget), name);
          if (action)
            break;
        }

      widget = gtk_widget_get_parent (widget);
    }

  /*
   * Now try to lookup the action from the workspace up, which is the case if
   * we don't have an active tab.
   */
  if (!action)
    {
      GbWorkbench *workbench;
      GbWorkspace *workspace;

      workbench = gb_command_provider_get_workbench (provider);
      workspace = gb_workbench_get_active_workspace (workbench);
      widget = GTK_WIDGET (workspace);

      while (widget)
        {
          if (G_IS_ACTION_MAP (widget))
            {
              action = g_action_map_lookup_action (G_ACTION_MAP (widget), name);
              if (action)
                break;
            }
          else if (GB_IS_WORKSPACE (widget))
            {
              GActionGroup *group;

              group = gb_workspace_get_actions (GB_WORKSPACE (widget));
              if (G_IS_ACTION_MAP (group))
                {
                  action = g_action_map_lookup_action (G_ACTION_MAP (group), name);
                  if (action)
                    break;
                }
            }

          widget = gtk_widget_get_parent (widget);
        }
    }

  /*
   * Now try to lookup the action inside of the GApplication.
   * This is useful for stuff like "quit", and "preferences".
   */
  if (!action)
    {
      GApplication *app;

      app = g_application_get_default ();
      action = g_action_map_lookup_action (G_ACTION_MAP (app), name);
    }

  if (!action && parameters)
    {
      g_variant_unref (parameters);
      parameters = NULL;
    }

  if (action)
    {
      command = gb_command_new ();
      if (parameters)
        g_object_set_data_full (G_OBJECT (command), "parameters",
                                g_variant_ref (parameters),
                                (GDestroyNotify)g_variant_unref);
      g_object_set_data_full (G_OBJECT (command), "action",
                              g_object_ref (action), g_object_unref);
      g_signal_connect (command, "execute", G_CALLBACK (execute_action), NULL);
    }


cleanup:
  g_free (name);

  RETURN (command);
}

static void
add_completions_from_group (GPtrArray         *completions,
                            const gchar       *prefix,
                            GActionGroup      *group)
{
  gchar **actions = g_action_group_list_actions (group);
  int i;

  for (i = 0; actions[i] != NULL; i++)
    {
      if (g_str_has_prefix (actions[i], prefix))
        g_ptr_array_add (completions, g_strdup (actions[i]));
    }

  g_strfreev (actions);
}

static void
gb_command_gaction_provider_complete (GbCommandProvider *provider,
                                      GPtrArray         *completions,
                                      const gchar       *initial_command_text)
{
  GbCommandGactionProvider *self = (GbCommandGactionProvider *)provider;
  GtkWidget *widget;
  const gchar *tmp;
  gchar *prefix;
  GApplication *app;
  GbWorkbench *workbench;
  GbWorkspace *workspace;

  ENTRY;

  g_return_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (self));

  tmp = initial_command_text;

  while (*tmp != 0 && *tmp != ' ' && *tmp != '(')
    tmp++;

  if (*tmp != 0)
    return;

  prefix = g_strndup (initial_command_text, tmp - initial_command_text);

  widget = GTK_WIDGET (gb_command_provider_get_active_tab (provider));
  while (widget)
    {
      if (G_IS_ACTION_GROUP (widget))
        add_completions_from_group (completions, prefix, G_ACTION_GROUP (widget));

      widget = gtk_widget_get_parent (widget);
    }

  /*
   * Now try to lookup the action from the workspace up, which is the case if
   * we don't have an active tab.
   */
  workbench = gb_command_provider_get_workbench (provider);
  workspace = gb_workbench_get_active_workspace (workbench);
  widget = GTK_WIDGET (workspace);

  while (widget)
    {
      if (G_IS_ACTION_GROUP (widget))
        add_completions_from_group (completions, prefix, G_ACTION_GROUP (widget));
      else if (GB_IS_WORKSPACE (widget))
        add_completions_from_group (completions, prefix,
                                   gb_workspace_get_actions (GB_WORKSPACE (widget)));

      widget = gtk_widget_get_parent (widget);
    }

  /*
   * Now try to lookup the action inside of the GApplication.
   * This is useful for stuff like "quit", and "preferences".
   */
  app = g_application_get_default ();
  add_completions_from_group (completions, prefix, G_ACTION_GROUP (app));

  g_free (prefix);

  RETURN();
}

static void
gb_command_gaction_provider_class_init (GbCommandGactionProviderClass *klass)
{
  GbCommandProviderClass *provider_class = GB_COMMAND_PROVIDER_CLASS (klass);

  provider_class->lookup = gb_command_gaction_provider_lookup;
  provider_class->complete = gb_command_gaction_provider_complete;
}

static void
gb_command_gaction_provider_init (GbCommandGactionProvider *self)
{
}
