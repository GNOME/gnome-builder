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

#include <ide.h>
#include <string.h>

#include "gb-command-gaction-provider.h"
#include "gb-command-gaction.h"

G_DEFINE_TYPE (GbCommandGactionProvider, gb_command_gaction_provider,
               GB_TYPE_COMMAND_PROVIDER)

GbCommandProvider *
gb_command_gaction_provider_new (GbWorkbench *workbench)
{
  return g_object_new (GB_TYPE_COMMAND_GACTION_PROVIDER,
                       "workbench", workbench,
                       NULL);
}

static GList *
discover_groups (GbCommandGactionProvider *provider)
{
  GbDocumentView *view;
  GApplication *application;
  GbWorkbench *workbench;
  GtkWidget *widget;
  GList *list = NULL;

  g_return_val_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (provider), NULL);

  view = gb_command_provider_get_active_view (GB_COMMAND_PROVIDER (provider));

  for (widget = GTK_WIDGET (view);
       widget;
       widget = gtk_widget_get_parent (widget))
    {
      const gchar **prefixes;
      guint i;

      prefixes = gtk_widget_list_action_prefixes (widget);

      if (prefixes)
        {
          GActionGroup *group;

          for (i = 0; prefixes [i]; i++)
            {
              group = gtk_widget_get_action_group (widget, prefixes [i]);

              if (G_IS_ACTION_GROUP (group))
                list = g_list_append (list, group);
            }

          g_free (prefixes);
        }
    }

  workbench = gb_command_provider_get_workbench (GB_COMMAND_PROVIDER (provider));
  list = g_list_append (list, G_ACTION_GROUP (workbench));

  application = g_application_get_default ();
  list = g_list_append (list, G_ACTION_GROUP (application));

  return list;
}

static gboolean
parse_command_text (const gchar  *command_text,
                    gchar       **name,
                    GVariant    **params)
{
  GVariant *ret_params = NULL;
  const gchar *str;
  gchar *tmp;
  gchar **parts;
  gchar *ret_name = NULL;

  g_return_val_if_fail (command_text, FALSE);
  g_return_val_if_fail (name, FALSE);
  g_return_val_if_fail (params, FALSE);

  *name = NULL;
  *params = NULL;

  /* Determine the command name */
  tmp = g_strdelimit (g_strdup (command_text), "(", ' ');
  parts = g_strsplit (tmp, " ", 2);
  ret_name = g_strdup (parts [0]);
  g_free (tmp);
  g_strfreev (parts);

  /* Advance to the (optional) parameters */
  for (str = command_text + strlen (ret_name);
       *str;
       str = g_utf8_next_char (str))
    {
      gunichar ch;

      ch = g_utf8_get_char (str);
      if (g_unichar_isspace (ch))
        continue;

      break;
    }

  /* Parse any (optional) parameters */
  if (*str != '\0')
    {
      ret_params = g_variant_parse (NULL, str, NULL, NULL, NULL);
      if (!ret_params)
        goto failure;
    }

  *name = ret_name;
  *params = ret_params;

  return TRUE;

failure:
  g_clear_pointer (&ret_name, g_free);
  g_clear_pointer (&ret_params, g_variant_unref);

  return FALSE;
}

static GbCommand *
gb_command_gaction_provider_lookup (GbCommandProvider *provider,
                                    const gchar       *command_text)
{
  GbCommandGactionProvider *self = (GbCommandGactionProvider *)provider;
  GbCommand *command = NULL;
  GVariant *params = NULL;
  GList *groups;
  GList *iter;
  gchar *action_name = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (self), NULL);
  g_return_val_if_fail (command_text, NULL);

  if (!parse_command_text (command_text, &action_name, &params))
    IDE_RETURN (NULL);

  groups = discover_groups (self);

  for (iter = groups; iter; iter = iter->next)
    {
      GActionGroup *group = G_ACTION_GROUP (iter->data);

      if (g_action_group_has_action (group, action_name))
        {
          command = g_object_new (GB_TYPE_COMMAND_GACTION,
                                  "action-group", group,
                                  "action-name", action_name,
                                  "parameters", params,
                                  NULL);
          break;
        }
    }

  g_clear_pointer (&params, g_variant_unref);
  g_list_free (groups);
  g_free (action_name);

  IDE_RETURN (command);
}

static void
gb_command_gaction_provider_complete (GbCommandProvider *provider,
                                      GPtrArray         *completions,
                                      const gchar       *initial_command_text)
{
  GbCommandGactionProvider *self = (GbCommandGactionProvider *)provider;
  GList *groups;
  GList *iter;

  IDE_ENTRY;

  g_return_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (self));
  g_return_if_fail (initial_command_text);

  groups = discover_groups (self);

  for (iter = groups; iter; iter = iter->next)
    {
      GActionGroup *group = iter->data;
      gchar **names;
      guint i;

      g_assert (G_IS_ACTION_GROUP (group));

      names = g_action_group_list_actions (group);

      for (i = 0; names [i]; i++)
        {
          if (g_str_has_prefix (names [i], initial_command_text))
            g_ptr_array_add (completions, g_strdup (names [i]));
        }

      g_free (names);
    }

  g_list_free (groups);

  IDE_EXIT;
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
