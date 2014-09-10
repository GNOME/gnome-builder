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

#include "gb-devhelp-workspace.h"
#include "gb-editor-workspace.h"
#include "gb-log.h"
#include "gb-widget.h"
#include "gb-workbench.h"
#include "gedit-menu-stack-switcher.h"

#define UI_RESOURCE_PATH "/org/gnome/builder/ui/gb-workbench.ui"

struct _GbWorkbenchPrivate
{
  GbWorkspace            *active_workspace;
  GbWorkspace            *devhelp;
  GbWorkspace            *editor;
  GtkMenuButton          *add_button;
  GtkButton              *back_button;
  GeditMenuStackSwitcher *gear_menu_button;
  GtkButton              *new_tab;
  GtkButton              *next_button;
  GtkButton              *run_button;
  GtkHeaderBar           *header_bar;
  GtkStack               *stack;
  GtkStackSwitcher       *switcher;
};

enum {
  PROP_0,
  LAST_PROP
};

enum {
  WORKSPACE_CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GbWorkbench,
                            gb_workbench,
                            GTK_TYPE_APPLICATION_WINDOW)

static guint gSignals[LAST_SIGNAL];

static void gb_workbench_action_set (GbWorkbench *workbench,
                                     const gchar *action_name,
                                     const gchar *first_property,
                                     ...) G_GNUC_NULL_TERMINATED;

static void
gb_workbench_action_set (GbWorkbench *workbench,
                         const gchar *action_name,
                         const gchar *first_property,
                         ...)
{
  GAction *action;
  va_list args;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (action_name);
  g_return_if_fail (first_property);

  action = g_action_map_lookup_action (G_ACTION_MAP (workbench), action_name);
  if (!action)
    {
      g_warning ("No such action: \"%s\"", action_name);
      return;
    }

  va_start (args, first_property);
  g_object_set_valist (G_OBJECT (action), first_property, args);
  va_end (args);
}

static void
gb_workbench_workspace_changed (GbWorkbench *workbench,
                                GbWorkspace *workspace)
{
  GbWorkbenchPrivate *priv;
  gboolean new_tab_enabled = FALSE;
  gboolean find_enabled = FALSE;

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
      new_tab_enabled = !!GB_WORKSPACE_GET_CLASS (workspace)->new_tab;
      find_enabled = !!GB_WORKSPACE_GET_CLASS (workspace)->find;
      g_object_add_weak_pointer (G_OBJECT (priv->active_workspace),
                                 (gpointer *) &priv->active_workspace);
    }

  gb_workbench_action_set (workbench, "new-tab",
                           "enabled", new_tab_enabled,
                           NULL);
  gb_workbench_action_set (workbench, "find",
                           "enabled", find_enabled,
                           NULL);

  EXIT;
}

static void
gb_workbench_activate_new_tab (GSimpleAction *action,
                               GVariant      *parameters,
                               gpointer       user_data)
{
  GbWorkbench *workbench = user_data;
  GbWorkspace *workspace;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (workbench->priv->active_workspace);

  workspace = workbench->priv->active_workspace;

  if (GB_WORKSPACE_GET_CLASS (workspace)->new_tab)
    GB_WORKSPACE_GET_CLASS (workspace)->new_tab (workspace);
}

static void
gb_workbench_activate_find (GSimpleAction *action,
                            GVariant      *parameters,
                            gpointer       user_data)
{
  GbWorkbench *workbench = user_data;
  GbWorkspace *workspace;

  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (workbench->priv->active_workspace);

  workspace = workbench->priv->active_workspace;

  if (GB_WORKSPACE_GET_CLASS (workspace)->find)
    GB_WORKSPACE_GET_CLASS (workspace)->find (workspace);
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
load_actions (GbWorkbench *workbench,
              GbWorkspace *workspace)
{
  GActionGroup *group;
  const gchar *name;

  group = gb_workspace_get_actions (workspace);
  name = gtk_widget_get_name (GTK_WIDGET (workspace));

  g_assert (name);

  if (group)
    {
      g_message ("Registering actions for \"%s\" prefix.", name);
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
gb_workbench_finalize (GObject *object)
{
  ENTRY;
  G_OBJECT_CLASS (gb_workbench_parent_class)->finalize (object);
  EXIT;
}

static void
gb_workbench_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  switch (prop_id)
    {
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

  object_class->finalize = gb_workbench_finalize;
  object_class->get_property = gb_workbench_get_property;
  object_class->set_property = gb_workbench_set_property;

  widget_class->realize = gb_workbench_realize;

  klass->workspace_changed = gb_workbench_workspace_changed;

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
                                                devhelp);
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
                                                switcher);
  gtk_widget_class_bind_template_child_private (widget_class, GbWorkbench,
                                                stack);

  g_type_ensure (GB_TYPE_EDITOR_WORKSPACE);
  g_type_ensure (GB_TYPE_DEVHELP_WORKSPACE);
  g_type_ensure (GEDIT_TYPE_MENU_STACK_SWITCHER);
}

static void
gb_workbench_init (GbWorkbench *workbench)
{
  static const GActionEntry action_entries[] = {
    { "new-tab", gb_workbench_activate_new_tab },
    { "find", gb_workbench_activate_find },
  };
  GbWorkbenchPrivate *priv;

  priv = workbench->priv = gb_workbench_get_instance_private (workbench);

  g_action_map_add_action_entries (G_ACTION_MAP (workbench), action_entries,
                                   G_N_ELEMENTS (action_entries), workbench);

  gtk_widget_init_template (GTK_WIDGET (workbench));

  g_signal_connect_object (priv->stack,
                           "notify::visible-child",
                           G_CALLBACK (gb_workbench_stack_child_changed),
                           workbench,
                           (G_CONNECT_SWAPPED | G_CONNECT_AFTER));

  gb_workbench_stack_child_changed (workbench, NULL, priv->stack);

  load_actions (workbench, GB_WORKSPACE (priv->editor));
  load_actions (workbench, GB_WORKSPACE (priv->devhelp));
}
