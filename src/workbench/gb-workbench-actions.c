/* gb-workbench-actions.c
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

#include <glib/gi18n.h>

#include "gb-workbench-actions.h"

enum
{
   PROP_0,
   PROP_WORKBENCH,
   LAST_PROP
};

struct _GbWorkbenchActionsPrivate
{
   GbWorkspace *workspace;
   GSList      *bindings;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbWorkbenchActions,
                            gb_workbench_actions,
                            G_TYPE_SIMPLE_ACTION_GROUP)

static GParamSpec *gParamSpecs [LAST_PROP];

GbWorkbenchActions *
gb_workbench_actions_new (GbWorkbench *workbench)
{
   return g_object_new (GB_TYPE_WORKBENCH_ACTIONS,
                        "workbench", workbench,
                        NULL);
}

static void
disconnect_bindings (GbWorkbenchActions *actions)
{
   GSList *iter;
   GSList *list;

   g_return_if_fail (GB_IS_WORKBENCH_ACTIONS (actions));

   list = actions->priv->bindings;
   actions->priv->bindings = NULL;

   for (iter = list; iter; iter = iter->next) {
      g_binding_unbind (iter->data);
   }

   g_slist_free (list);
}

static void
connect_bindings (GbWorkbenchActions *actions,
                  GbWorkspace        *workspace)
{
   GbWorkbenchActionsPrivate *priv;
   GActionGroup *workspace_actions;
   GBinding *binding;
   GAction *action;
   GAction *workspace_action;
   gchar **names;
   guint i;

   g_return_if_fail (GB_IS_WORKBENCH_ACTIONS (actions));
   g_return_if_fail (GB_IS_WORKSPACE (workspace));

   priv = actions->priv;

   workspace_actions = gb_workspace_get_actions (workspace);

   names = g_action_group_list_actions (G_ACTION_GROUP (actions));

   for (i = 0; names [i]; i++) {
      action = g_action_map_lookup_action (G_ACTION_MAP (actions),
                                           names [i]);
      workspace_action = g_action_map_lookup_action (G_ACTION_MAP (workspace_actions),
                                                     names [i]);

      if (workspace_action) {
         binding = g_object_bind_property (workspace_action, "enabled",
                                           action, "enabled",
                                           G_BINDING_SYNC_CREATE);
         priv->bindings = g_slist_prepend (priv->bindings, binding);
      } else {
          g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
      }
   }
}

static void
on_workspace_changed (GbWorkbench        *workbench,
                      GbWorkspace        *workspace,
                      GbWorkbenchActions *actions)
{
   GbWorkbenchActionsPrivate *priv;

   g_return_if_fail (GB_IS_WORKBENCH_ACTIONS (actions));
   g_return_if_fail (!workspace || GB_IS_WORKSPACE (workspace));
   g_return_if_fail (GB_IS_WORKBENCH (workbench));

   priv = actions->priv;

   disconnect_bindings (actions);

   if (priv->workspace) {
      g_object_remove_weak_pointer (G_OBJECT (priv->workspace),
                                    (gpointer *)&priv->workspace);
   }

   priv->workspace = gb_workbench_get_active_workspace (workbench);

   if (priv->workspace) {
      g_object_add_weak_pointer (G_OBJECT (priv->workspace),
                                 (gpointer *)&priv->workspace);
      connect_bindings (actions, priv->workspace);
   }
}

static void
proxy_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
   GbWorkbenchActionsPrivate *priv;
   GbWorkbenchActions *actions = user_data;
   GActionGroup *workspace_actions;
   const gchar *action_name;
   GAction *proxy;

   g_assert (GB_IS_WORKBENCH_ACTIONS (actions));

   priv = actions->priv;

   if (priv->workspace) {
      workspace_actions = gb_workspace_get_actions (priv->workspace);
      action_name = g_action_get_name (G_ACTION (action));
      proxy = g_action_map_lookup_action (G_ACTION_MAP (workspace_actions),
                                          action_name);
      if (proxy) {
         g_action_activate (proxy, parameter);
      }
   }
}

static void
gb_workbench_actions_set_workbench (GbWorkbenchActions *actions,
                                    GbWorkbench        *workbench)
{
   g_return_if_fail (GB_IS_WORKBENCH_ACTIONS (actions));
   g_return_if_fail (GB_IS_WORKBENCH (workbench));

   g_signal_connect_object (workbench,
                            "workspace-changed",
                            G_CALLBACK (on_workspace_changed),
                            actions,
                            0);
}

static void
gb_workbench_actions_constructed (GObject *object)
{
   GbWorkbenchActions *actions = (GbWorkbenchActions *)object;
   static const GActionEntry action_entries[] = {
      { "open", proxy_action },
      { "save", proxy_action },
      { "save-as", proxy_action },
   };

   g_assert (GB_IS_WORKBENCH_ACTIONS (actions));

   g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                    action_entries,
                                    G_N_ELEMENTS (action_entries),
                                    actions);
}

static void
gb_workbench_actions_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
   GbWorkbenchActions *actions = GB_WORKBENCH_ACTIONS (object);

   switch (prop_id) {
   case PROP_WORKBENCH:
      gb_workbench_actions_set_workbench (actions, g_value_get_object (value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
   }
}

static void
gb_workbench_actions_class_init (GbWorkbenchActionsClass *klass)
{
   GObjectClass *object_class = G_OBJECT_CLASS (klass);

   object_class->constructed = gb_workbench_actions_constructed;
   object_class->set_property = gb_workbench_actions_set_property;

   gParamSpecs [PROP_WORKBENCH] =
      g_param_spec_object ("workbench",
                           _("Workbench"),
                           _("The workbench for the actions."),
                           GB_TYPE_WORKBENCH,
                           (G_PARAM_WRITABLE |
                            G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_STATIC_STRINGS));
   g_object_class_install_property (object_class, PROP_WORKBENCH,
                                    gParamSpecs [PROP_WORKBENCH]);
}

static void
gb_workbench_actions_init (GbWorkbenchActions *actions)
{
   actions->priv = gb_workbench_actions_get_instance_private (actions);
}
