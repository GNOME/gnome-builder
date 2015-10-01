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
#include <libpeas/peas.h>

#include "gb-sysmon-addin.h"
#include "gb-sysmon-panel.h"
#include "gb-sysmon-resources.h"
#include "gb-workbench-addin.h"
#include "gb-workspace.h"

struct _GbSysmonAddin
{
  GObject      parent_instance;
  GtkWidget   *panel;
};

static void workbench_addin_iface_init (GbWorkbenchAddinInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GbSysmonAddin, gb_sysmon_addin, G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (GB_TYPE_WORKBENCH_ADDIN,
                                                               workbench_addin_iface_init))

static void
gb_sysmon_addin_load (GbWorkbenchAddin *addin,
                      GbWorkbench      *workbench)
{
  GbSysmonAddin *self = (GbSysmonAddin *)addin;
  GtkWidget *workspace;
  GtkWidget *pane;
  GtkWidget *panel;

  g_assert (GB_IS_SYSMON_ADDIN (self));
  g_assert (GB_IS_WORKBENCH (workbench));

  workspace = gb_workbench_get_workspace (workbench);
  pane = gb_workspace_get_bottom_pane (GB_WORKSPACE (workspace));
  panel = g_object_new (GB_TYPE_SYSMON_PANEL, "visible", TRUE, NULL);
  ide_set_weak_pointer (&self->panel, panel);
  gb_workspace_pane_add_page (GB_WORKSPACE_PANE (pane),
                              GTK_WIDGET (panel),
                              _("System Monitor"),
                              "utilities-system-monitor-symbolic");
}

static void
gb_sysmon_addin_unload (GbWorkbenchAddin *addin,
                        GbWorkbench      *workbench)
{
  g_assert (GB_IS_SYSMON_ADDIN (addin));
  g_assert (GB_IS_WORKBENCH (workbench));
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
}

static void
gb_sysmon_addin_class_finalize (GbSysmonAddinClass *klass)
{
}

static void
gb_sysmon_addin_init (GbSysmonAddin *self)
{
}

void
peas_register_types (PeasObjectModule *module)
{
  gb_sysmon_addin_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              GB_TYPE_WORKBENCH_ADDIN,
                                              GB_TYPE_SYSMON_ADDIN);
}
