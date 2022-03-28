/* ide-gui-global.c
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

#define G_LOG_DOMAIN "ide-gui-global"

#include "config.h"

#include <libide-threading.h>

#include "ide-gui-global.h"
#include "ide-gui-private.h"
#include "ide-workspace.h"

static GQuark quark_handler;
static GQuark quark_where_context_was;

static void ide_widget_notify_context    (GtkWidget  *toplevel,
                                          GParamSpec *pspec,
                                          GtkWidget  *widget);
static void ide_widget_hierarchy_changed (GtkWidget  *widget,
                                          GtkWidget  *previous_toplevel,
                                          gpointer    user_data);

static void
ide_widget_notify_context (GtkWidget  *toplevel,
                           GParamSpec *pspec,
                           GtkWidget  *widget)
{
  IdeWidgetContextHandler handler;
  IdeContext *old_context;
  IdeContext *context;

  g_assert (GTK_IS_WIDGET (toplevel));
  g_assert (GTK_IS_WIDGET (widget));

  handler = g_object_get_qdata (G_OBJECT (widget), quark_handler);
  old_context = g_object_get_qdata (G_OBJECT (widget), quark_where_context_was);

  if (handler == NULL)
    return;

  context = ide_widget_get_context (toplevel);

  if (context == old_context)
    return;

  g_object_set_qdata (G_OBJECT (widget), quark_where_context_was, context);

  g_signal_handlers_disconnect_by_func (toplevel,
                                        G_CALLBACK (ide_widget_notify_context),
                                        widget);
  g_signal_handlers_disconnect_by_func (widget,
                                        G_CALLBACK (ide_widget_hierarchy_changed),
                                        NULL);

  handler (widget, context);
}

static gboolean
has_context_property (GtkWidget *widget)
{
  GParamSpec *pspec;

  g_assert (GTK_IS_WIDGET (widget));

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (widget), "context");
  return pspec != NULL && g_type_is_a (pspec->value_type, IDE_TYPE_CONTEXT);
}

static void
ide_widget_hierarchy_changed (GtkWidget *widget,
                              GtkWidget *previous_toplevel,
                              gpointer   user_data)
{
  GtkWidget *toplevel;

  g_assert (GTK_IS_WIDGET (widget));

  if (GTK_IS_WINDOW (previous_toplevel))
    g_signal_handlers_disconnect_by_func (previous_toplevel,
                                          G_CALLBACK (ide_widget_notify_context),
                                          widget);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel) && has_context_property (toplevel))
    {
      g_signal_connect_object (toplevel,
                               "notify::context",
                               G_CALLBACK (ide_widget_notify_context),
                               widget,
                               0);
      ide_widget_notify_context (toplevel, NULL, widget);
    }
}

/**
 * ide_widget_set_context_handler:
 * @widget: (type Gtk.Widget): a #GtkWidget
 * @handler: (scope async): A callback to handle the context
 *
 * Calls @handler when the #IdeContext has been set for @widget.
 *
 * Since: 3.32
 */
void
ide_widget_set_context_handler (gpointer                widget,
                                IdeWidgetContextHandler handler)
{
  GtkWidget *toplevel;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* Ensure we have our quarks for quick key lookup */
  if G_UNLIKELY (quark_handler == 0)
    quark_handler = g_quark_from_static_string ("IDE_CONTEXT_HANDLER");

  if G_UNLIKELY (quark_where_context_was == 0)
    quark_where_context_was = g_quark_from_static_string ("IDE_CONTEXT");

  g_object_set_qdata (G_OBJECT (widget), quark_handler, handler);

  g_signal_connect (widget,
                    "hierarchy-changed",
                    G_CALLBACK (ide_widget_hierarchy_changed),
                    NULL);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    ide_widget_hierarchy_changed (widget, NULL, NULL);
}

/**
 * ide_widget_get_context:
 * @widget: a #GtkWidget
 *
 * Gets the context for the widget.
 *
 * Returns: (nullable) (transfer none): an #IdeContext, or %NULL
 *
 * Since: 3.32
 */
IdeContext *
ide_widget_get_context (GtkWidget *widget)
{
  GtkWidget *toplevel;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  toplevel = gtk_widget_get_toplevel (widget);

  if (IDE_IS_WORKSPACE (toplevel))
    return ide_workspace_get_context (IDE_WORKSPACE (toplevel));

  return NULL;
}

/**
 * ide_widget_get_workbench:
 * @widget: a #GtkWidget
 *
 * Gets the #IdeWorkbench that contains @widget.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkbench or %NULL
 *
 * Since: 3.32
 */
IdeWorkbench *
ide_widget_get_workbench (GtkWidget *widget)
{
  GtkWidget *toplevel;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    {
      GtkWindowGroup *group = gtk_window_get_group (GTK_WINDOW (toplevel));

      if (IDE_IS_WORKBENCH (group))
        return IDE_WORKBENCH (group);
    }

  return NULL;
}

/**
 * ide_widget_get_workspace:
 * @widget: a #GtkWidget
 *
 * Gets the #IdeWorkspace containing @widget.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkspace or %NULL
 *
 * Since: 3.32
 */
IdeWorkspace *
ide_widget_get_workspace (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return IDE_WORKSPACE (gtk_widget_get_root (widget));
}

static void
show_parents (GtkWidget *widget)
{
  GtkWidget *workspace;
  GtkWidget *parent;

  g_assert (GTK_IS_WIDGET (widget));

  workspace = gtk_widget_get_ancestor (widget, IDE_TYPE_WORKSPACE);
  parent = gtk_widget_get_parent (widget);

  if (DZL_IS_DOCK_REVEALER (widget))
    dzl_dock_revealer_set_reveal_child (DZL_DOCK_REVEALER (widget), TRUE);

  if (IDE_IS_SURFACE (widget))
    ide_workspace_set_visible_surface (IDE_WORKSPACE (workspace), IDE_SURFACE (widget));

  if (GTK_IS_STACK (parent))
    gtk_stack_set_visible_child (GTK_STACK (parent), widget);

  if (parent != NULL)
    show_parents (parent);
}

void
ide_widget_reveal_and_grab (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  show_parents (widget);
  gtk_widget_grab_focus (widget);
}
