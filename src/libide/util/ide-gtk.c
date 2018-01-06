/* ide-gtk.c
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-gtk"

#include "application/ide-application.h"
#include "util/ide-gtk.h"

static void
ide_widget_notify_context (GtkWidget  *toplevel,
                           GParamSpec *pspec,
                           GtkWidget  *widget)
{
  IdeWidgetContextHandler handler;
  g_autoptr(IdeContext) context = NULL;

  handler = g_object_get_data (G_OBJECT (widget), "IDE_CONTEXT_HANDLER");
  if (handler == NULL)
    return;

  g_object_get (toplevel,
                "context", &context,
                NULL);

  handler (widget, context);
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

  if (GTK_IS_WINDOW (toplevel))
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

  g_object_set_data (G_OBJECT (widget), "IDE_CONTEXT_HANDLER", handler);

  g_signal_connect (widget,
                    "hierarchy-changed",
                    G_CALLBACK (ide_widget_hierarchy_changed),
                    NULL);

  if ((toplevel = gtk_widget_get_toplevel (widget)))
    ide_widget_hierarchy_changed (widget, NULL, NULL);
}

/**
 * ide_widget_get_workbench:
 *
 * Gets the workbench @widget is associated with, if any.
 *
 * If no workbench is associated, NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeWorkbench
 */
IdeWorkbench *
ide_widget_get_workbench (GtkWidget *widget)
{
  GtkWidget *ancestor;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  ancestor = gtk_widget_get_ancestor (widget, IDE_TYPE_WORKBENCH);
  if (IDE_IS_WORKBENCH (ancestor))
    return IDE_WORKBENCH (ancestor);

  /*
   * TODO: Add "IDE_WORKBENCH" gdata for popout windows.
   */

  return NULL;
}

/**
 * ide_widget_get_context: (skip)
 *
 * Returns: (nullable) (transfer none): An #IdeContext or %NULL.
 */
IdeContext *
ide_widget_get_context (GtkWidget *widget)
{
  IdeWorkbench *workbench;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  workbench = ide_widget_get_workbench (widget);

  if (workbench == NULL)
    return NULL;

  return ide_workbench_get_context (workbench);
}

void
ide_widget_message (gpointer     instance,
                    const gchar *format,
                    ...)
{
  g_autofree gchar *str = NULL;
  IdeContext *context = NULL;
  va_list args;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (!instance || GTK_IS_WIDGET (instance));

  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  if (instance != NULL)
    context = ide_widget_get_context (instance);

  if (context != NULL)
    ide_context_emit_log (context, G_LOG_LEVEL_MESSAGE, str, -1);
  else
    g_message ("%s", str);
}

void
ide_widget_warning (gpointer     instance,
                    const gchar *format,
                    ...)
{
  g_autofree gchar *str = NULL;
  IdeContext *context = NULL;
  va_list args;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (!instance || GTK_IS_WIDGET (instance));

  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  if (instance != NULL)
    context = ide_widget_get_context (instance);

  if (context != NULL)
    ide_context_emit_log (context, G_LOG_LEVEL_WARNING, str, -1);
  else
    g_warning ("%s", str);
}
