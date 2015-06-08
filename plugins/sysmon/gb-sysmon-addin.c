/* gb-sysmon-addin.c
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

#include "gb-plugins.h"
#include "gb-sysmon-addin.h"
#include "gb-sysmon-panel.h"
#include "gb-sysmon-resources.h"
#include "gb-workbench-addin.h"
#include "gb-workspace.h"

struct _GbSysmonAddin
{
  GObject      parent_instance;

  GbWorkbench *workbench;
  GtkWidget   *panel;
};

static void workbench_addin_iface_init (GbWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbSysmonAddin, gb_sysmon_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GB_TYPE_WORKBENCH_ADDIN,
                                                workbench_addin_iface_init))

enum {
  PROP_0,
  PROP_WORKBENCH,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_sysmon_addin_load (GbWorkbenchAddin *addin)
{
  GbSysmonAddin *self = (GbSysmonAddin *)addin;
  GtkWidget *workspace;
  GtkWidget *pane;

  g_assert (GB_IS_SYSMON_ADDIN (self));

  workspace = gb_workbench_get_workspace (self->workbench);
  pane = gb_workspace_get_bottom_pane (GB_WORKSPACE (workspace));
  self->panel = g_object_new (GB_TYPE_SYSMON_PANEL, "visible", TRUE, NULL);
  gb_workspace_pane_add_page (GB_WORKSPACE_PANE (pane),
                              GTK_WIDGET (self->panel),
                              _("System Monitor"),
                              "utilities-system-monitor-symbolic");
}

static void
gb_sysmon_addin_unload (GbWorkbenchAddin *addin)
{
  GbSysmonAddin *self = (GbSysmonAddin *)addin;
  GtkWidget *parent;

  parent = gtk_widget_get_parent (self->panel);
  gtk_container_remove (GTK_CONTAINER (parent), self->panel);
  self->panel = NULL;
}

static void
gb_sysmon_addin_finalize (GObject *object)
{
  GbSysmonAddin *self = (GbSysmonAddin *)object;

  ide_clear_weak_pointer (&self->workbench);

  G_OBJECT_CLASS (gb_sysmon_addin_parent_class)->finalize (object);
}

static void
gb_sysmon_addin_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbSysmonAddin *self = GB_SYSMON_ADDIN (object);

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
workbench_addin_iface_init (GbWorkbenchAddinInterface *iface)
{
  iface->load = gb_sysmon_addin_load;
  iface->unload = gb_sysmon_addin_unload;
}

static void
gb_sysmon_addin_class_init (GbSysmonAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_sysmon_addin_finalize;
  object_class->set_property = gb_sysmon_addin_set_property;

  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("Workbench"),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_sysmon_addin_init (GbSysmonAddin *self)
{
}

GB_DEFINE_EMBEDDED_PLUGIN (gb_sysmon,
                           gb_sysmon_get_resource (),
                           "resource:///org/gnome/builder/plugins/sysmon/gb-sysmon.plugin",
                           GB_DEFINE_PLUGIN_TYPE (GB_TYPE_WORKBENCH_ADDIN, GB_TYPE_SYSMON_ADDIN))
