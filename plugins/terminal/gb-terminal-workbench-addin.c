/* gb-terminal-workbench-addin.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
#include <ide.h>

#include "gb-terminal-view.h"
#include "gb-terminal-workbench-addin.h"

struct _GbTerminalWorkbenchAddin
{
  GObject         parent_instance;

  IdeWorkbench   *workbench;

  GbTerminalView *panel_terminal;
  GtkWidget      *panel_dock_widget;
};

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbTerminalWorkbenchAddin,
                        gb_terminal_workbench_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
new_terminal_activate_cb (GSimpleAction            *action,
                          GVariant                 *param,
                          GbTerminalWorkbenchAddin *self)
{
  GbTerminalView *view;
  IdePerspective *perspective;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));

  perspective = ide_workbench_get_perspective_by_name (self->workbench, "editor");
  ide_workbench_set_visible_perspective (self->workbench, perspective);

  view = g_object_new (GB_TYPE_TERMINAL_VIEW,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (perspective), GTK_WIDGET (view));
  ide_workbench_focus (self->workbench, GTK_WIDGET (view));
}

static void
gb_terminal_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbTerminalWorkbenchAddin *self = (GbTerminalWorkbenchAddin *)addin;
  IdePerspective *perspective;
  GtkWidget *bottom_pane;
  g_autoptr(GSimpleAction) action = NULL;

  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  ide_set_weak_pointer (&self->workbench, workbench);

  action = g_simple_action_new ("new-terminal", NULL);
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (new_terminal_activate_cb),
                           self,
                           0);
  g_action_map_add_action (G_ACTION_MAP (workbench), G_ACTION (action));

  if (self->panel_terminal == NULL)
    {
      self->panel_dock_widget = g_object_new (PNL_TYPE_DOCK_WIDGET,
                                              "expand", TRUE,
                                              "title", _("Terminal"),
                                              "visible", TRUE,
                                              NULL);
      self->panel_terminal = g_object_new (GB_TYPE_TERMINAL_VIEW,
                                           "visible", TRUE,
                                           NULL);
      gtk_container_add (GTK_CONTAINER (self->panel_dock_widget),
                         GTK_WIDGET (self->panel_terminal));

      g_object_add_weak_pointer (G_OBJECT (self->panel_terminal),
                                 (gpointer *)&self->panel_terminal);
      g_object_add_weak_pointer (G_OBJECT (self->panel_dock_widget),
                                 (gpointer *)&self->panel_dock_widget);
    }

  perspective = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (IDE_IS_LAYOUT (perspective));

  bottom_pane = pnl_dock_bin_get_bottom_edge (PNL_DOCK_BIN (perspective));
  gtk_container_add (GTK_CONTAINER (bottom_pane), GTK_WIDGET (self->panel_dock_widget));
}

static void
gb_terminal_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbTerminalWorkbenchAddin *self = (GbTerminalWorkbenchAddin *)addin;

  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));

  g_action_map_remove_action (G_ACTION_MAP (self->workbench), "new-terminal");

  if (self->panel_dock_widget != NULL)
    {
      gtk_widget_destroy (self->panel_dock_widget);
      self->panel_dock_widget = NULL;
    }
}

static void
gb_terminal_workbench_addin_class_init (GbTerminalWorkbenchAddinClass *klass)
{
}

static void
gb_terminal_workbench_addin_init (GbTerminalWorkbenchAddin *self)
{
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gb_terminal_workbench_addin_load;
  iface->unload = gb_terminal_workbench_addin_unload;
}
