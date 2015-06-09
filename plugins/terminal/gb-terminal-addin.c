/* gb-terminal-addin.c
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

#include "gb-terminal.h"
#include "gb-terminal-addin.h"
#include "gb-view-grid.h"
#include "gb-workspace.h"

struct _GbTerminalAddin
{
  GObject      parent_instance;

  GbWorkbench *workbench;
  GbTerminal  *panel_terminal;
};

static void workbench_addin_iface_init (GbWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbTerminalAddin, gb_terminal_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GB_TYPE_WORKBENCH_ADDIN,
                                                workbench_addin_iface_init))

enum {
  PROP_0,
  PROP_WORKBENCH,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
new_terminal_activate_cb (GSimpleAction   *action,
                          GVariant        *param,
                          GbTerminalAddin *self)
{
  GtkWidget *terminal;
  GtkWidget *grid;
  GtkWidget *stack;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GB_IS_TERMINAL_ADDIN (self));

  grid = gb_workbench_get_view_grid (self->workbench);
  terminal = g_object_new (GB_TYPE_TERMINAL,
                           "visible", TRUE,
                           NULL);
  stack = gb_view_grid_get_last_focus (GB_VIEW_GRID (grid));
  gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (terminal));
  gtk_widget_grab_focus (GTK_WIDGET (terminal));
}

static void
gb_terminal_addin_load (GbWorkbenchAddin *addin)
{
  GbTerminalAddin *self = (GbTerminalAddin *)addin;
  GbWorkspace *workspace;
  GtkWidget *bottom_pane;
  g_autoptr(GSimpleAction) action = NULL;

  g_assert (GB_IS_TERMINAL_ADDIN (self));
  g_assert (GB_IS_WORKBENCH (self->workbench));

  action = g_simple_action_new ("new-terminal", NULL);
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (new_terminal_activate_cb),
                           self,
                           0);
  g_action_map_add_action (G_ACTION_MAP (self->workbench), G_ACTION (action));

  if (self->panel_terminal == NULL)
    {
      self->panel_terminal = g_object_new (GB_TYPE_TERMINAL,
                                           "visible", TRUE,
                                           NULL);
      g_object_add_weak_pointer (G_OBJECT (self->panel_terminal),
                                 (gpointer *)&self->panel_terminal);
    }

  workspace = GB_WORKSPACE (gb_workbench_get_workspace (self->workbench));
  bottom_pane = gb_workspace_get_bottom_pane (workspace);
  gb_workspace_pane_add_page (GB_WORKSPACE_PANE (bottom_pane),
                              GTK_WIDGET (self->panel_terminal),
                              _("Terminal"),
                              "utilities-terminal-symbolic");
}

static void
gb_terminal_addin_unload (GbWorkbenchAddin *addin)
{
  GbTerminalAddin *self = (GbTerminalAddin *)addin;

  g_assert (GB_IS_TERMINAL_ADDIN (self));

  g_action_map_remove_action (G_ACTION_MAP (self->workbench), "new-terminal");

  if (self->panel_terminal != NULL)
    {
      GtkWidget *parent;

      parent = gtk_widget_get_parent (GTK_WIDGET (self->panel_terminal));
      gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (self->panel_terminal));
    }
}

static void
gb_terminal_addin_finalize (GObject *object)
{
  GbTerminalAddin *self = (GbTerminalAddin *)object;

  ide_clear_weak_pointer (&self->workbench);

  G_OBJECT_CLASS (gb_terminal_addin_parent_class)->finalize (object);
}

static void
gb_terminal_addin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbTerminalAddin *self = GB_TERMINAL_ADDIN (object);

  switch (prop_id)
    {
    case PROP_WORKBENCH:
      ide_set_weak_pointer (&self->workbench, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_terminal_addin_class_init (GbTerminalAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_terminal_addin_finalize;
  object_class->set_property = gb_terminal_addin_set_property;

  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("The workbench window."),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_terminal_addin_init (GbTerminalAddin *self)
{
}

static void
workbench_addin_iface_init (GbWorkbenchAddinInterface *iface)
{
  iface->load = gb_terminal_addin_load;
  iface->unload = gb_terminal_addin_unload;
}
