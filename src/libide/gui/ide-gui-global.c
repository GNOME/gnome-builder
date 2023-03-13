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
#include "ide-workspace.h"

static GQuark quark_handler;
static GQuark quark_where_context_was;

static void ide_widget_notify_context (GtkWidget  *toplevel,
                                       GParamSpec *pspec,
                                       GtkWidget  *widget);
static void ide_widget_notify_root_cb (GtkWidget  *widget,
                                       GParamSpec *pspec,
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
                                        G_CALLBACK (ide_widget_notify_root_cb),
                                        NULL);

  handler (widget, context);
}

static gboolean
has_context_property (GtkWidget *widget)
{
  GParamSpec *pspec;

  g_assert (GTK_IS_WIDGET (widget));

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (widget), "context");

  return pspec != NULL &&
         (pspec->flags & G_PARAM_READABLE) != 0 &&
         g_type_is_a (pspec->value_type, IDE_TYPE_CONTEXT);
}

static void
ide_widget_notify_root_cb (GtkWidget  *widget,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
  GtkWidget *toplevel;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (user_data == NULL);

  toplevel = GTK_WIDGET (gtk_widget_get_native (widget));

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
                    "notify::root",
                    G_CALLBACK (ide_widget_notify_root_cb),
                    NULL);

  toplevel = GTK_WIDGET (gtk_widget_get_native (widget));

  if (GTK_IS_WINDOW (toplevel))
    ide_widget_notify_root_cb (widget, NULL, NULL);
}

static gboolean dummy_cb (gpointer data) { return G_SOURCE_REMOVE; }

/**
 * ide_widget_get_context:
 * @widget: a #GtkWidget
 *
 * Gets the context for the widget.
 *
 * Returns: (nullable) (transfer none): an #IdeContext, or %NULL
 */
IdeContext *
ide_widget_get_context (GtkWidget *widget)
{
  GtkRoot *root;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  root = gtk_widget_get_root (widget);

  if (IDE_IS_WORKSPACE (root))
    return ide_workspace_get_context (IDE_WORKSPACE (root));

  if (GTK_IS_WINDOW (root))
    {
      GtkWindowGroup *group = gtk_window_get_group (GTK_WINDOW (root));

      if (IDE_IS_WORKBENCH (group))
        return ide_workbench_get_context (IDE_WORKBENCH (group));
    }

  if (root != NULL)
    {
      GObjectClass *object_class = G_OBJECT_GET_CLASS (root);
      GParamSpec *pspec = g_object_class_find_property (object_class, "context");

      if (G_IS_PARAM_SPEC_OBJECT (pspec) &&
          g_type_is_a (pspec->value_type, IDE_TYPE_CONTEXT))
        {
          g_auto(GValue) value = G_VALUE_INIT;
          IdeContext *ret;

          g_value_init (&value, IDE_TYPE_CONTEXT);
          g_object_get_property (G_OBJECT (root), "context", &value);

          ret = g_value_dup_object (&value);
          g_idle_add_full (G_PRIORITY_LOW, dummy_cb, ret, g_object_unref);

          return ret;
        }
    }

  return NULL;
}

/**
 * ide_widget_get_workbench:
 * @widget: a #GtkWidget
 *
 * Gets the #IdeWorkbench that contains @widget.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkbench or %NULL
 */
IdeWorkbench *
ide_widget_get_workbench (GtkWidget *widget)
{
  GtkWindow *transient_for;
  GtkRoot *root;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (GTK_IS_ROOT (widget))
    root = GTK_ROOT (widget);
  else
    root = gtk_widget_get_root (widget);

  if (root != NULL &&
      !IDE_IS_WORKSPACE (root) &&
      GTK_IS_WINDOW (root) &&
      (transient_for = gtk_window_get_transient_for (GTK_WINDOW (root))))
    return ide_widget_get_workbench (GTK_WIDGET (transient_for));

  if (GTK_IS_WINDOW (root))
    {
      GtkWindowGroup *group = gtk_window_get_group (GTK_WINDOW (root));

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
 */
IdeWorkspace *
ide_widget_get_workspace (GtkWidget *widget)
{
  GtkWindow *transient_for;
  GtkRoot *root;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (GTK_IS_ROOT (widget))
    root = GTK_ROOT (widget);
  else
    root = gtk_widget_get_root (widget);

  if (root != NULL &&
      !IDE_IS_WORKSPACE (root) &&
      GTK_IS_WINDOW (root) &&
      (transient_for = gtk_window_get_transient_for (GTK_WINDOW (root))))
    return ide_widget_get_workspace (GTK_WIDGET (transient_for));

  return IDE_IS_WORKSPACE (root) ? IDE_WORKSPACE (root) : NULL;
}
