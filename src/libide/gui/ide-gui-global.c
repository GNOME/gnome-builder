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

#include <dazzle.h>
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

  return (IdeWorkspace *)dzl_gtk_widget_get_relative (widget, IDE_TYPE_WORKSPACE);
}

static gboolean
ide_gtk_progress_bar_tick_cb (gpointer data)
{
  GtkProgressBar *progress = data;

  g_assert (GTK_IS_PROGRESS_BAR (progress));

  gtk_progress_bar_pulse (progress);
  gtk_widget_queue_draw (GTK_WIDGET (progress));

  return G_SOURCE_CONTINUE;
}

void
_ide_gtk_progress_bar_stop_pulsing (GtkProgressBar *progress)
{
  guint tick_id;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (progress));

  tick_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (progress), "PULSE_ID"));

  if (tick_id != 0)
    {
      g_source_remove (tick_id);
      g_object_set_data (G_OBJECT (progress), "PULSE_ID", NULL);
    }

  gtk_progress_bar_set_fraction (progress, 0.0);
}

void
_ide_gtk_progress_bar_start_pulsing (GtkProgressBar *progress)
{
  guint tick_id;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (progress));

  if (g_object_get_data (G_OBJECT (progress), "PULSE_ID"))
    return;

  gtk_progress_bar_set_fraction (progress, 0.0);
  gtk_progress_bar_set_pulse_step (progress, .5);

  /* We want lower than the frame rate, because that is all that is needed */
  tick_id = dzl_frame_source_add_full (G_PRIORITY_DEFAULT,
                                       2,
                                       ide_gtk_progress_bar_tick_cb,
                                       g_object_ref (progress),
                                       g_object_unref);
  g_object_set_data (G_OBJECT (progress), "PULSE_ID", GUINT_TO_POINTER (tick_id));
  ide_gtk_progress_bar_tick_cb (progress);
}

static void
ide_gtk_show_uri_on_window_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    g_warning ("Subprocess failed: %s", error->message);
}

gboolean
ide_gtk_show_uri_on_window (GtkWindow    *window,
                            const gchar  *uri,
                            gint64        timestamp,
                            GError      **error)
{
  g_return_val_if_fail (!window || GTK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  if (ide_is_flatpak ())
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autoptr(IdeSubprocess) subprocess = NULL;

      /* We can't currently trust gtk_show_uri_on_window() because it tries
       * to open our HTML page with Builder inside our current flatpak
       * environment! We need to ensure this is fixed upstream, but it's
       * currently unclear how to do so since we register handles for html.
       */

      launcher = ide_subprocess_launcher_new (0);
      ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
      ide_subprocess_launcher_set_clear_env (launcher, FALSE);
      ide_subprocess_launcher_push_argv (launcher, "xdg-open");
      ide_subprocess_launcher_push_argv (launcher, uri);

      if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, error)))
        return FALSE;

      ide_subprocess_wait_async (subprocess,
                                 NULL,
                                 ide_gtk_show_uri_on_window_cb,
                                 NULL);
    }
  else
    {
      /* XXX: Workaround for wayland timestamp issue */
      if (!gtk_show_uri_on_window (window, uri, timestamp / 1000L, error))
        return FALSE;
    }

  return TRUE;
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

void
ide_gtk_window_present (GtkWindow *window)
{
  /* TODO: We need the last event time to do this properly. Until then,
   * we'll just fake some timing info to workaround wayland issues.
   */
  gtk_window_present_with_time (window, g_get_monotonic_time () / 1000L);
}

static void
split_action_name (const gchar  *action_name,
                   gchar       **prefix,
                   gchar       **name)
{
  const gchar *dot;

  g_assert (prefix != NULL);
  g_assert (name != NULL);

  *prefix = NULL;
  *name = NULL;

  if (action_name == NULL)
    return;

  dot = strchr (action_name, '.');

  if (dot == NULL)
    *name = g_strdup (action_name);
  else
    {
      *prefix = g_strndup (action_name, dot - action_name);
      *name = g_strdup (dot + 1);
    }
}

gboolean
_ide_gtk_widget_action_is_stateful (GtkWidget   *widget,
                                    const gchar *action_name)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *prefix = NULL;
  GtkWidget *toplevel;
  GApplication *app;
  GActionGroup *group = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (action_name, FALSE);

  split_action_name (action_name, &prefix, &name);

  app = g_application_get_default ();
  toplevel = gtk_widget_get_toplevel (widget);

  while ((group == NULL) && (widget != NULL))
    {
      group = gtk_widget_get_action_group (widget, prefix);

      if G_UNLIKELY (GTK_IS_POPOVER (widget))
        {
          GtkWidget *relative_to;

          relative_to = gtk_popover_get_relative_to (GTK_POPOVER (widget));

          if (relative_to != NULL)
            widget = relative_to;
          else
            widget = gtk_widget_get_parent (widget);
        }
      else
        {
          widget = gtk_widget_get_parent (widget);
        }
    }

  if (!group && g_str_equal (prefix, "win") && G_IS_ACTION_GROUP (toplevel))
    group = G_ACTION_GROUP (toplevel);

  if (!group && g_str_equal (prefix, "app") && G_IS_ACTION_GROUP (app))
    group = G_ACTION_GROUP (app);

  if (group && g_action_group_has_action (group, name))
    return g_action_group_get_action_state_type (group, name) != NULL;

  return FALSE;
}
