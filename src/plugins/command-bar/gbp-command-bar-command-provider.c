/* gbp-command-bar-command-provider.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-command-bar-command-provider"

#include <libide-gui.h>
#include <libide-sourceview.h>
#include <libide-threading.h>

#include "gbp-command-bar-command-provider.h"
#include "gbp-gaction-command.h"

struct _GbpCommandBarCommandProvider
{
  IdeObject parent_instance;
};

static void
add_from_group (const gchar  *needle,
                GPtrArray    *results,
                const gchar  *prefix,
                GtkWidget    *widget,
                GActionGroup *group,
                GHashTable   *seen)
{
  g_auto(GStrv) actions = NULL;

  g_assert (needle != NULL);
  g_assert (results != NULL);
  g_assert (prefix != NULL);
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (G_IS_ACTION_GROUP (group));

  /* Skip source-view actions which bridge to signals */
  if (g_str_equal (prefix, "source-view"))
    return;

  actions = g_action_group_list_actions (group);

  for (guint j = 0; actions[j]; j++)
    {
      g_autofree gchar *title = NULL;
      const GVariantType *type;
      GbpGactionCommand *command;
      guint priority = 0;

      if (g_hash_table_contains (seen, actions[j]))
        continue;

      g_hash_table_insert (seen, g_strdup (actions[j]), NULL);

      /* Skip actions with params */
      if ((type = g_action_group_get_action_parameter_type (group, actions[j])))
        continue;

      if (!ide_completion_fuzzy_match (actions[j], needle, &priority))
        continue;

      title = ide_completion_fuzzy_highlight (actions[j], needle);
      command = gbp_gaction_command_new (widget, prefix, actions[j], NULL, title, (gint)priority);
      g_ptr_array_add (results, g_steal_pointer (&command));
    }
}

static void
populate_gactions_at_widget (const gchar *needle,
                             GPtrArray   *results,
                             GtkWidget   *widget,
                             GHashTable  *seen)
{
  g_autofree const gchar **prefixes = NULL;
  GtkWidget *parent;

  g_assert (results != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  if ((prefixes = gtk_widget_list_action_prefixes (widget)))
    {
      for (guint i = 0; prefixes[i]; i++)
        {
          GActionGroup *group;

          if ((group = gtk_widget_get_action_group (widget, prefixes[i])))
            add_from_group (needle, results, prefixes[i], widget, group, seen);
        }
    }

  if ((parent = gtk_widget_get_parent (widget)))
    populate_gactions_at_widget (needle, results, parent, seen);
  else
    add_from_group (needle, results, "app", widget, G_ACTION_GROUP (IDE_APPLICATION_DEFAULT), seen);
}

static void
gbp_command_bar_command_provider_query_async (IdeCommandProvider  *provider,
                                              IdeWorkspace        *workspace,
                                              const gchar         *typed_text,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  GbpCommandBarCommandProvider *self = (GbpCommandBarCommandProvider *)provider;
  g_autoptr(GHashTable) seen = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) results = NULL;
  g_autofree gchar *needle = NULL;
  IdeSurface *surface;
  IdePage *page;

  g_assert (GBP_IS_COMMAND_BAR_COMMAND_PROVIDER (self));
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_command_bar_command_provider_query_async);

  seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  needle = g_utf8_casefold (typed_text, -1);
  results = g_ptr_array_new_with_free_func (g_object_unref);
  surface = ide_workspace_get_visible_surface (workspace);

  if ((page = ide_workspace_get_most_recent_page (workspace)))
    populate_gactions_at_widget (needle, results, GTK_WIDGET (page), seen);
  else
    populate_gactions_at_widget (needle, results, GTK_WIDGET (surface), seen);

  ide_task_return_pointer (task,
                           g_steal_pointer (&results),
                           g_ptr_array_unref);
}

static GPtrArray *
gbp_command_bar_command_provider_query_finish (IdeCommandProvider  *provider,
                                               GAsyncResult        *result,
                                               GError             **error)
{
  GPtrArray *ret;

  g_assert (GBP_IS_COMMAND_BAR_COMMAND_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
command_provider_iface_init (IdeCommandProviderInterface *iface)
{
  iface->query_async = gbp_command_bar_command_provider_query_async;
  iface->query_finish = gbp_command_bar_command_provider_query_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpCommandBarCommandProvider,
                         gbp_command_bar_command_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMMAND_PROVIDER,
                                                command_provider_iface_init))

static void
gbp_command_bar_command_provider_class_init (GbpCommandBarCommandProviderClass *klass)
{
}

static void
gbp_command_bar_command_provider_init (GbpCommandBarCommandProvider *self)
{
}
