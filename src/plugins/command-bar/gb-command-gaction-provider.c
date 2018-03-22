/* gb-command-gaction-provider.c
 *
 * Copyright 2014 Christian Hergert <christian@hergert.me>
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
#include <glib.h>

#include "gb-command-gaction-provider.h"
#include "gb-command-gaction.h"

struct _GbCommandGactionProvider
{
  GbCommandProvider parent_instance;
};

G_DEFINE_TYPE (GbCommandGactionProvider, gb_command_gaction_provider, GB_TYPE_COMMAND_PROVIDER)

/* Set this to 1 to enable the debug helper which prints the
 * content of Action groups to standard output :
 *
 * it's triggered by going to the command bar then hitting 'tab'
 */

#if 0
#define GB_DEBUG_ACTIONS
#endif

typedef struct
{
  GActionGroup *group;
  gchar        *prefix;
} GbGroup;

typedef struct
{
  const gchar *command_name;
  const gchar *prefix;
  const gchar *action_name;
} GbActionCommandMap;

/* Used to print discovered prefixes and GActions for a GObject */
G_GNUC_UNUSED static void
show_prefix_actions (GObject *object)
{
  const gchar **prefixes;
  gchar **names;
  guint i, n;
  GActionGroup *group;

  if (GTK_IS_WIDGET (object))
    printf ("type: GtkWidget\n");
  else if (G_IS_OBJECT (object))
    printf ("type: GObject\n");
  else
    {
      printf ("type: Unknow\n");
      return;
    }

    printf ("  type name: %s\n", G_OBJECT_TYPE_NAME (object));

    if (GTK_IS_WIDGET (object))
      {
        prefixes = gtk_widget_list_action_prefixes (GTK_WIDGET (object));
        if (prefixes [0] != NULL)
          printf ("  prefixes:\n");

        for (i = 0; prefixes [i]; i++)
          {
            printf ("    - %s:\n", prefixes [i]);
            group = gtk_widget_get_action_group (GTK_WIDGET (object), prefixes [i]);
            if (group)
              {
                names = g_action_group_list_actions (group);
                if (names [0] != NULL)
                  printf ("        names:\n");

                for (n = 0; names [n]; n++)
                  printf ("          - %s\n", names [n]);

                printf ("\n");
              }
            else
              {
                printf ("        names: no names - group is NULL\n");
              }
          }

        printf ("\n");
      }
    else
      {
        names = g_action_group_list_actions (G_ACTION_GROUP (object));
        if (names [0] != NULL)
          printf ("        names:\n");

        for (n = 0; names [n]; n++)
          printf ("          - %s\n", names [n]);
      }
}

GbCommandProvider *
gb_command_gaction_provider_new (IdeWorkbench *workbench)
{
  return g_object_new (GB_TYPE_COMMAND_GACTION_PROVIDER,
                       "workbench", workbench,
                       NULL);
}

static GbGroup *
gb_group_new (GActionGroup *group, const gchar *prefix)
{
  GbGroup *gb_group;

  g_assert (group != NULL);
  g_assert (prefix != NULL && prefix [0] != '\0');

  gb_group = g_new (GbGroup, 1);

  gb_group->group = group;
  gb_group->prefix = g_strdup (prefix);

  return gb_group;
}

static void
gb_group_free (GbGroup *gb_group)
{
  g_assert (gb_group != NULL);

  g_free (gb_group->prefix);
  g_free (gb_group);
}

static GList *
discover_groups (GbCommandGactionProvider *provider)
{
  GApplication *application;
  GtkWidget *widget;
  GbGroup *gb_group = NULL;
  GList *list = NULL;
  GType type;

  g_return_val_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (provider), NULL);

  widget = (GtkWidget *)gb_command_provider_get_active_view (GB_COMMAND_PROVIDER (provider));

  if (widget == NULL)
    widget = (GtkWidget *)gb_command_provider_get_workbench (GB_COMMAND_PROVIDER (provider));

  for (; widget; widget = gtk_widget_get_parent (widget))
    {
      const gchar **prefixes;
      guint i;

#ifdef GB_DEBUG_ACTIONS
      show_prefix_actions (G_OBJECT (widget));
#endif

      /* We exclude these types, they're already in the widgets hierarchy */
      type = G_OBJECT_TYPE (widget);
      if (type == IDE_TYPE_EDITOR_VIEW)
        continue;

      prefixes = gtk_widget_list_action_prefixes (widget);

      if (prefixes)
        {
          for (i = 0; prefixes [i]; i++)
            {
              GActionGroup *group = gtk_widget_get_action_group (widget, prefixes [i]);

              if (G_IS_ACTION_GROUP (group))
                {
                  gb_group = gb_group_new (group, prefixes [i]);
                  list = g_list_append (list, gb_group);
                }
            }

          g_free (prefixes);
        }
    }

  application = g_application_get_default ();
  gb_group = gb_group_new (G_ACTION_GROUP (application), "app");
  list = g_list_append (list, gb_group);

#ifdef GB_DEBUG_ACTIONS
  show_prefix_actions (G_OBJECT (application));
#endif

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

/*
 * Command names mapping and masking:
 *
 * Format: command_name, prefix, action_name
 * command_name can be NULL so that the specific (prefix, action_name)
 * is masked in this case.
 *
 * As we allow a NULL command_name, you must closed the array with a triple NULL.
 */
static const GbActionCommandMap action_maps [] = {
  { "quitall",  "app",          "quit"  },
  { NULL,       "layoutgrid",   "close" },
  { NULL,       "layoutstack",  "close-view" },
  { NULL,       "editor-view",  "save" },
  { NULL,       "editor-view",  "save-as" },
  { NULL }
};

static gboolean
search_command_in_maps (const gchar  *action_name,
                        const gchar  *prefix,
                        const gchar **command_name)
{
  guint i;

  for (i = 0; (action_maps [i].prefix != NULL) && (action_maps [i].action_name != NULL); i++)
    {
      if (!g_strcmp0 (action_maps [i].prefix, prefix) &&
           g_str_equal (action_maps [i].action_name, action_name))
        {
          *command_name = action_maps [i].command_name;
          return TRUE;
        }
    }

  *command_name = NULL;
  return FALSE;
}

static gboolean
search_action_in_maps (const gchar  *command_name,
                       const gchar **action_name,
                       const gchar **prefix)
{
  guint i;

  for (i = 0; (action_maps [i].prefix != NULL) && (action_maps [i].action_name != NULL); i++)
    {
      if (action_maps [i].command_name == NULL)
        continue;

      if (!g_strcmp0 (command_name, action_maps [i].command_name))
        {
          *action_name = action_maps [i].action_name;
          *prefix = action_maps [i].prefix;

          return TRUE;
        }
    }

  return FALSE;
}

static GbCommand *
gb_command_gaction_provider_lookup (GbCommandProvider *provider,
                                    const gchar       *command_text)
{
  GbCommandGactionProvider *self = (GbCommandGactionProvider *)provider;
  GbCommand *command = NULL;
  GVariant *params = NULL;
  GList *gb_groups;
  GbGroup *gb_group = NULL;
  GActionGroup *group = NULL;
  GList *iter;
  gchar *command_name = NULL;
  const gchar *new_command_name = NULL;
  const gchar *action_name = NULL;
  const gchar *prefix = NULL;
  gboolean result = FALSE;

  IDE_ENTRY;

  g_return_val_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (self), NULL);
  g_return_val_if_fail (command_text, NULL);

  if (!parse_command_text (command_text, &command_name, &params))
    IDE_RETURN (NULL);

  gb_groups = discover_groups (self);

  if (search_action_in_maps (command_name, &action_name, &prefix))
    {
      /* We double-check that the action really exist */
      for (iter = gb_groups; iter; iter = iter->next)
        {
          gb_group = (GbGroup *)iter->data;
          group = G_ACTION_GROUP (gb_group->group);

          if (g_str_equal (prefix, gb_group->prefix) && g_action_group_has_action (group, action_name))
            {
              result = TRUE;
              break;
            }
        }
    }

  if (!result)
    {
      for (iter = gb_groups; iter; iter = iter->next)
        {
          gb_group = (GbGroup *)iter->data;
          group = G_ACTION_GROUP (gb_group->group);

          if (g_action_group_has_action (group, command_name))
            {
              /* We must be sure that the action is not masked or overridden */
              prefix = gb_group->prefix;
              if (search_command_in_maps (command_name, prefix, &new_command_name))
                {
                  result = FALSE;
                  break;
                }

              action_name = command_name;
              result = TRUE;
              break;
            }
        }
    }

  if (result)
    {
      command = g_object_new (GB_TYPE_COMMAND_GACTION,
                              "action-group", group,
                              "action-name", action_name,
                              "parameters", params,
                              NULL);
    }

  g_clear_pointer (&params, g_variant_unref);
  g_free (command_name);
  g_list_free_full (gb_groups, (GDestroyNotify)gb_group_free);

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
      GbGroup *gb_group = iter->data;
      GActionGroup *group = gb_group->group;
      gchar *prefix = gb_group->prefix;
      g_auto(GStrv) names = NULL;
      const gchar *command_name;
      guint i;

      g_assert (G_IS_ACTION_GROUP (group));

      names = g_action_group_list_actions (group);

      for (i = 0; names [i]; i++)
        {
          if (search_command_in_maps (names [i], prefix, &command_name))
            {
              if (command_name != NULL && g_str_has_prefix (command_name, initial_command_text))
                g_ptr_array_add (completions, g_strdup (command_name));

              continue;
            }

          if (g_str_has_prefix (names [i], initial_command_text) &&
              g_action_group_get_action_enabled (group, names [i]))
            g_ptr_array_add (completions, g_strdup (names [i]));
        }
    }

  g_list_free_full (groups, (GDestroyNotify)gb_group_free);

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
