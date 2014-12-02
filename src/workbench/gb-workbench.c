/* gb-workbench.c
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

#define G_LOG_DOMAIN "workbench"

#include <glib/gi18n.h>

#include "gb-command-bar.h"
#include "gb-command-gaction-provider.h"
#include "gb-command-manager.h"
#include "gb-command-vim-provider.h"
#include "gb-credits-widget.h"
#include "gb-editor-workspace.h"
#include "gb-log.h"
#include "gb-widget.h"
#include "gb-workbench.h"
#include "gb-workbench-actions.h"
#include "gedit-menu-stack-switcher.h"

#define UI_RESOURCE_PATH "/org/gnome/builder/ui/gb-workbench.ui"

struct _GbWorkbenchPrivate
{
  GbWorkbenchActions     *actions;
  GbCommandManager       *command_manager;
  GbNavigationList       *navigation_list;

  GbWorkspace            *active_workspace;
  GbCommandBar           *command_bar;
  GbCreditsWidget        *credits;
  GbWorkspace            *editor;
  GtkMenuButton          *add_button;
  GtkButton              *back_button;
  GeditMenuStackSwitcher *gear_menu_button;
  GtkButton              *new_tab;
  GtkButton              *next_button;
  GtkButton              *run_button;
  GtkHeaderBar           *header_bar;
  GtkSearchEntry         *search_entry;
  GtkStack               *stack;
  GtkStackSwitcher       *switcher;
};

enum {
  PROP_0,
  PROP_COMMAND_MANAGER,
  PROP_NAVIGATION_LIST,
  LAST_PROP
};

enum {
  WORKSPACE_CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GbWorkbench,
                            gb_workbench,
                            GTK_TYPE_APPLICATION_WINDOW)

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

/**
 * gb_workbench_get_command_manager:
 *
 * Retrieves the command manager for the workspace.
 *
 * Returns: (transfer none) (type GbCommandManager*): A #GbCommandManager.
 */
gpointer
gb_workbench_get_command_manager (GbWorkbench *workbench)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (workbench), NULL);

  return workbench->priv->command_manager;
}

/**
 * gb_workbench_get_navigation_list:
 *
 * Fetches the navigation list for the workbench. This can be used to move
 * between edit points between workspaces.
 *
 * Returns: (transfer none): A #GbNavigationlist.
 */
GbNavigationList *
gb_workbench_get_navigation_list (GbWorkbench *workbench)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (workbench), NULL);

  return workbench->priv->navigation_list;
}

GbWorkspace *
gb_workbench_get_active_workspace (GbWorkbench *workbench)
{
   GtkWidget *child;

   g_return_val_if_fail (GB_IS_WORKBENCH (workbench), NULL);

   child = gtk_stack_get_visible_child (workbench->priv->stack);

   return GB_WORKSPACE (child);
}

GbWorkspace *
gb_workbench_get_workspace (GbWorkbench *workbench,
                            GType        type)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (workbench), NULL);
  g_return_val_if_fail (g_type_is_a (type, GB_TYPE_WORKSPACE), NULL);

  if (type == GB_TYPE_EDITOR_WORKSPACE)
    return GB_WORKSPACE (workbench->priv->editor);

  return NULL;
}

void
gb_workbench_roll_credits (GbWorkbench *workbench)
{
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  gb_credits_widget_start (workbench->priv->credits);
}

static void
gb_workbench_workspace_changed (GbWorkbench *workbench,
                                GbWorkspace *workspace)
{
  GbWorkbenchPrivate *priv;

  ENTRY;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (GB_IS_WORKSPACE (workspace));

  priv = workbench->priv;

  if (priv->active_workspace)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->active_workspace),
                                    (gpointer *) &priv->active_workspace);
      priv->active_workspace = NULL;
    }

  if (workspace)
    {
      priv->active_workspace = workspace;
      g_object_add_weak_pointer (G_OBJECT (priv->active_workspace),
                                 (gpointer *) &priv->active_workspace);
      gtk_widget_grab_focus (GTK_WIDGET (workspace));
    }

  EXIT;
}

static void
gb_workbench_stack_child_changed (GbWorkbench *workbench,
                                  GParamSpec  *pspec,
                                  GtkStack    *stack)
{
  GtkWidget *child;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (GTK_IS_STACK (stack));

  child = gtk_stack_get_visible_child (stack);
  g_assert (!child || GB_IS_WORKSPACE (child));

  if (child)
    g_signal_emit (workbench, gSignals[WORKSPACE_CHANGED], 0, child);
}

static void
gb_workbench_load_workspace_actions (GbWorkbench *workbench,
                                     GbWorkspace *workspace)
{
  GActionGroup *group;
  const gchar *name;

  group = gb_workspace_get_actions (workspace);
  name = gtk_widget_get_name (GTK_WIDGET (workspace));

  g_assert (name);

  if (group)
    {
      g_message (_("Registering actions for \"%s\" prefix."), name);
      gtk_widget_insert_action_group (GTK_WIDGET (workbench), name, group);
    }
}

static void
gb_workbench_realize (GtkWidget *widget)
{
  GbWorkbench *workbench = (GbWorkbench *)widget;

  if (GTK_WIDGET_CLASS (gb_workbench_parent_class)->realize)
    GTK_WIDGET_CLASS (gb_workbench_parent_class)->realize (widget);

  gtk_widget_grab_focus (GTK_WIDGET (workbench->priv->editor));
}

static void
on_workspace1_activate (GSimpleAction *action,
                        GVariant      *variant,
                        gpointer       user_data)
{
  GbWorkbenchPrivate *priv = GB_WORKBENCH (user_data)->priv;
  GtkWidget *child = gtk_stack_get_child_by_name (priv->stack, "editor");
  gtk_stack_set_visible_child (priv->stack, child);
  gtk_widget_grab_focus (child);
}

static void
on_go_forward_activate (GSimpleAction *action,
                        GVariant      *variant,
                        gpointer       user_data)
{
  GbWorkbenchPrivate *priv;
  GbWorkbench *workbench = user_data;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  priv = workbench->priv;

  if (gb_navigation_list_get_can_go_forward (priv->navigation_list))
    gb_navigation_list_go_forward (priv->navigation_list);
}

static void
on_go_backward_activate (GSimpleAction *action,
                         GVariant      *variant,
                         gpointer       user_data)
{
  GbWorkbenchPrivate *priv;
  GbWorkbench *workbench = user_data;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  priv = workbench->priv;

  if (gb_navigation_list_get_can_go_backward (priv->navigation_list))
    gb_navigation_list_go_backward (priv->navigation_list);
}

static void
gb_workbench_navigation_changed (GbWorkbench      *workbench,
                                 GParamSpec       *pspec,
                                 GbNavigationList *list)
{
  GbWorkbenchPrivate *priv;
  GbNavigationItem *item;
  GbWorkspace *workspace;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (GB_IS_NAVIGATION_LIST (list));

  priv = workbench->priv;

  item = gb_navigation_list_get_current_item (list);

  if (item)
    {
      workspace = gb_navigation_item_get_workspace (item);
      if (workspace)
        gtk_stack_set_visible_child (priv->stack, GTK_WIDGET (workspace));
      gb_navigation_item_activate (item);
    }
}

static void
on_show_command_bar_activate (GSimpleAction *action,
                              GVariant      *parameters,
                              gpointer       user_data)
{
  GbWorkbenchPrivate *priv;
  GbWorkbench *workbench = user_data;
  gboolean show = TRUE;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  priv = workbench->priv;

  show = !gtk_revealer_get_reveal_child (GTK_REVEALER (priv->command_bar));

  if (show)
    gb_command_bar_show (priv->command_bar);
  else
    gb_command_bar_hide (priv->command_bar);
}

static void
on_toggle_command_bar_activate (GSimpleAction *action,
                                GVariant      *parameters,
                                gpointer       user_data)
{
  GbWorkbenchPrivate *priv;
  GbWorkbench *workbench = user_data;
  gboolean show = TRUE;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  priv = workbench->priv;

  show = g_variant_get_boolean (parameters);

  if (show)
    gb_command_bar_show (priv->command_bar);
  else
    gb_command_bar_hide (priv->command_bar);
}

static void
on_command_bar_notify_child_revealed (GbCommandBar *command_bar,
                                      GParamSpec   *pspec,
                                      GbWorkbench  *workbench)
{
  gboolean reveal_child;

  g_return_if_fail (GB_IS_COMMAND_BAR (command_bar));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  reveal_child = gtk_revealer_get_reveal_child (GTK_REVEALER (command_bar));

  if (!reveal_child && workbench->priv->active_workspace)
    gtk_widget_grab_focus (GTK_WIDGET (workbench->priv->active_workspace));
}

static void
on_global_search_activate (GSimpleAction *action,
                           GVariant      *parameters,
                           gpointer       user_data)
{
  GbWorkbench *workbench = user_data;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  gtk_widget_grab_focus (GTK_WIDGET (workbench->priv->search_entry));
}

static void
on_roll_credits (GSimpleAction *action,
                 GVariant      *parameters,
                 gpointer       user_data)
{
  GbWorkbench *workbench = user_data;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  gb_workbench_roll_credits (workbench);
}

static void
gb_workbench_constructed (GObject *object)
{
  static const GActionEntry actions[] = {
    { "workspace1", on_workspace1_activate },
    { "global-search", on_global_search_activate },
    { "go-backward", on_go_backward_activate },
    { "go-forward", on_go_forward_activate },
    { "show-command-bar", on_show_command_bar_activate },
    { "toggle-command-bar", on_toggle_command_bar_activate, "b" },
    { "roll-credits", on_roll_credits },
  };
  GbWorkbenchPrivate *priv;
  GbWorkbench *workbench = (GbWorkbench *)object;
  GtkApplication *app;
  GAction *action;
  GMenu *menu;

  g_assert (GB_IS_WORKBENCH (workbench));

  ENTRY;

  priv = workbench->priv;

  gb_workbench_load_workspace_actions (workbench, GB_WORKSPACE (priv->editor));

  app = GTK_APPLICATION (g_application_get_default ());
  menu = gtk_application_get_menu_by_id (app, "gear-menu");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->gear_menu_button),
                                  G_MENU_MODEL (menu));

  g_signal_connect_object (priv->stack,
                           "notify::visible-child",
                           G_CALLBACK (gb_workbench_stack_child_changed),
                           workbench,
                           (G_CONNECT_SWAPPED | G_CONNECT_AFTER));

  gb_workbench_stack_child_changed (workbench, NULL, priv->stack);

  g_action_map_add_action_entries (G_ACTION_MAP (workbench), actions,
                                   G_N_ELEMENTS (actions), workbench);

  g_signal_connect_object (priv->navigation_list,
                           "notify::current-item",
                           G_CALLBACK (gb_workbench_navigation_changed),
                           workbench,
                           G_CONNECT_SWAPPED);

  action = g_action_map_lookup_action (G_ACTION_MAP (workbench), "go-backward");
  g_object_bind_property (priv->navigation_list, "can-go-backward",
                          action, "enabled", G_BINDING_SYNC_CREATE);

  action = g_action_map_lookup_action (G_ACTION_MAP (workbench), "go-forward");
  g_object_bind_property (priv->navigation_list, "can-go-forward",
                          action, "enabled", G_BINDING_SYNC_CREATE);

  g_signal_connect (priv->command_bar, "notify::child-revealed",
                    G_CALLBACK (on_command_bar_notify_child_revealed),
                    workbench);

  G_OBJECT_CLASS (gb_workbench_parent_class)->constructed (object);

  EXIT;
}

static void
gb_workbench_dispose (GObject *object)
{
  GbWorkbenchPrivate *priv;

  ENTRY;

  priv = GB_WORKBENCH (object)->priv;

  g_clear_object (&priv->actions);
  g_clear_object (&priv->command_manager);
  g_clear_object (&priv->navigation_list);

  G_OBJECT_CLASS (gb_workbench_parent_class)->dispose (object);

  EXIT;
}

static void
gb_workbench_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbWorkbench *self = (GbWorkbench *)object;

  switch (prop_id)
    {
    case PROP_COMMAND_MANAGER:
      g_value_set_object (value, gb_workbench_get_command_manager (self));
      break;

    case PROP_NAVIGATION_LIST:
      g_value_set_object (value, gb_workbench_get_navigation_list (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workbench_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workbench_class_init (GbWorkbenchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_workbench_constructed;
  object_class->dispose = gb_workbench_dispose;
  object_class->get_property = gb_workbench_get_property;
  object_class->set_property = gb_workbench_set_property;

  widget_class->realize = gb_workbench_realize;

  klass->workspace_changed = gb_workbench_workspace_changed;

  gParamSpecs [PROP_COMMAND_MANAGER] =
    g_param_spec_object ("command-manager",
                         _("Command Manager"),
                         _("The command manager for the workspace."),
                         GB_TYPE_COMMAND_MANAGER,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_COMMAND_MANAGER,
                                   gParamSpecs [PROP_COMMAND_MANAGER]);

  gParamSpecs [PROP_NAVIGATION_LIST] =
    g_param_spec_object ("navigation-list",
                         _("Navigation List"),
                         _("The navigation list for the workbench."),
                         GB_TYPE_NAVIGATION_LIST,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NAVIGATION_LIST,
                                   gParamSpecs [PROP_NAVIGATION_LIST]);

  gSignals [WORKSPACE_CHANGED] =
    g_signal_new ("workspace-changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GbWorkbenchClass, workspace_changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_WORKSPACE);

  gtk_widget_class_set_template_from_resource (widget_class, UI_RESOURCE_PATH);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                command_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                credits);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                editor);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                add_button);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                back_button);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                gear_menu_button);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                new_tab);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                next_button);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                run_button);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                header_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                switcher);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                stack);

  g_type_ensure (GB_TYPE_COMMAND_BAR);
  g_type_ensure (GB_TYPE_CREDITS_WIDGET);
  g_type_ensure (GB_TYPE_EDITOR_WORKSPACE);
  g_type_ensure (GEDIT_TYPE_MENU_STACK_SWITCHER);
}

static void
gb_workbench_init (GbWorkbench *workbench)
{
  GbCommandProvider *provider;

  workbench->priv = gb_workbench_get_instance_private (workbench);

  gtk_widget_init_template (GTK_WIDGET (workbench));

  workbench->priv->command_manager =
    g_object_new (GB_TYPE_COMMAND_MANAGER,
                  NULL);

  provider = g_object_new (GB_TYPE_COMMAND_GACTION_PROVIDER,
                           "workbench", workbench,
                           NULL);
  gb_command_manager_add_provider (workbench->priv->command_manager, provider);

  provider = g_object_new (GB_TYPE_COMMAND_VIM_PROVIDER,
                           "workbench", workbench,
                           NULL);
  gb_command_manager_add_provider (workbench->priv->command_manager, provider);

  workbench->priv->navigation_list = g_object_new (GB_TYPE_NAVIGATION_LIST,
                                                   "workbench", workbench,
                                                   NULL);
  workbench->priv->actions = gb_workbench_actions_new (workbench);
  gtk_widget_insert_action_group (GTK_WIDGET (workbench),
                                  "workbench",
                                  G_ACTION_GROUP (workbench->priv->actions));
}
