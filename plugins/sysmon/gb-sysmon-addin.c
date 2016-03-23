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
#include <ide.h>

#include "gb-sysmon-addin.h"
#include "gb-sysmon-panel.h"
#include "gb-sysmon-resources.h"

struct _GbSysmonAddin
{
  GObject      parent_instance;
  GtkWidget   *panel;
};

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GbSysmonAddin, gb_sysmon_addin, G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (IDE_TYPE_WORKBENCH_ADDIN,
                                                               workbench_addin_iface_init))

static void
gb_sysmon_addin_load (IdeWorkbenchAddin *addin,
                      IdeWorkbench      *workbench)
{
  GbSysmonAddin *self = (GbSysmonAddin *)addin;
  IdePerspective *editor;
  GtkWidget *pane;
  GtkWidget *panel;

  g_assert (GB_IS_SYSMON_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  editor = ide_workbench_get_perspective_by_name (workbench, "editor");

  g_assert (editor != NULL);
  g_assert (IDE_IS_LAYOUT (editor));

  pane = pnl_dock_bin_get_bottom_edge (PNL_DOCK_BIN (editor));
  panel = g_object_new (GB_TYPE_SYSMON_PANEL,
                        "expand", TRUE,
                        "visible", TRUE,
                        NULL);
  ide_set_weak_pointer (&self->panel, panel);
  gtk_container_add (GTK_CONTAINER (pane), GTK_WIDGET (panel));
}

static void
gb_sysmon_addin_unload (IdeWorkbenchAddin *addin,
                        IdeWorkbench      *workbench)
{
  GbSysmonAddin *self = (GbSysmonAddin *)addin;

  g_assert (GB_IS_SYSMON_ADDIN (addin));
  g_assert (IDE_IS_WORKBENCH (workbench));

  if (self->panel != NULL)
    {
      gtk_widget_destroy (self->panel);
      ide_clear_weak_pointer (&self->panel);
    }
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
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
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              GB_TYPE_SYSMON_ADDIN);
}
