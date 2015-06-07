/* gb-devhelp-panel.c
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
#include <devhelp/devhelp.h>

#include "gb-devhelp-panel.h"
#include "gb-devhelp-resources.h"
#include "gb-plugins.h"
#include "gb-workbench-addin.h"
#include "gb-workbench.h"
#include "gb-workspace.h"

struct _GbDevhelpPanel
{
  GtkBin         parent_instance;

  GbWorkbench   *workbench;
  DhBookManager *book_manager;

  GtkWidget     *sidebar;
};

static void workbench_addin_iface_init (GbWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbDevhelpPanel, gb_devhelp_panel, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GB_TYPE_WORKBENCH_ADDIN,
                                                workbench_addin_iface_init))

enum {
  PROP_0,
  PROP_WORKBENCH,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_devhelp_panel_load (GbWorkbenchAddin *addin)
{
  GbDevhelpPanel *self = (GbDevhelpPanel *)addin;
  GtkWidget *workspace;
  GtkWidget *pane;

  g_print ("Loading and adding panel to workbench\n");

  g_assert (GB_IS_DEVHELP_PANEL (self));

  workspace = gb_workbench_get_workspace (self->workbench);
  pane = gb_workspace_get_right_pane (GB_WORKSPACE (workspace));
  gb_workspace_pane_add_page (GB_WORKSPACE_PANE (pane), GTK_WIDGET (self),
                              _("Documentation"),
                              GTK_STOCK_HELP);

  gtk_widget_show (GTK_WIDGET (self));
}

static void
gb_devhelp_panel_unload (GbWorkbenchAddin *addin)
{
  GbDevhelpPanel *self = (GbDevhelpPanel *)addin;

  g_assert (GB_IS_DEVHELP_PANEL (self));

  ide_clear_weak_pointer (&self->workbench);
}

static void
gb_devhelp_panel_finalize (GObject *object)
{
  GbDevhelpPanel *self = (GbDevhelpPanel *)object;

  ide_clear_weak_pointer (&self->workbench);

  g_clear_object (&self->book_manager);

  G_OBJECT_CLASS (gb_devhelp_panel_parent_class)->finalize (object);
}

static void
gb_devhelp_panel_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbDevhelpPanel *self = GB_DEVHELP_PANEL (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_devhelp_panel_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbDevhelpPanel *self = GB_DEVHELP_PANEL (object);

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
gb_devhelp_panel_class_init (GbDevhelpPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_devhelp_panel_finalize;
  object_class->get_property = gb_devhelp_panel_get_property;
  object_class->set_property = gb_devhelp_panel_set_property;

  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("Workbench"),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_devhelp_panel_init (GbDevhelpPanel *self)
{
  self->book_manager = dh_book_manager_new ();
  dh_book_manager_populate (self->book_manager);

  self->sidebar = dh_sidebar_new (self->book_manager);
  gtk_container_add (GTK_CONTAINER (self), self->sidebar);
  gtk_widget_show (self->sidebar);
}

static void
workbench_addin_iface_init (GbWorkbenchAddinInterface *iface)
{
  iface->load = gb_devhelp_panel_load;
  iface->unload = gb_devhelp_panel_unload;
}

GB_DEFINE_EMBEDDED_PLUGIN (gb_devhelp,
                           gb_devhelp_get_resource (),
                           "resource:///org/gnome/builder/plugins/devhelp/gb-devhelp.plugin",
                           GB_DEFINE_PLUGIN_TYPE (GB_TYPE_WORKBENCH_ADDIN, GB_TYPE_DEVHELP_PANEL))
