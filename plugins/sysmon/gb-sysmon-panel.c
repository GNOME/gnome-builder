/* gb-sysmon-panel.c
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

#include <realtime-graphs.h>

#include "gb-sysmon-panel.h"

struct _GbSysmonPanel
{
  PnlDockWidget  parent_instance;
  RgCpuGraph    *cpu_graph;
};

G_DEFINE_TYPE (GbSysmonPanel, gb_sysmon_panel, PNL_TYPE_DOCK_WIDGET)

static void
gb_sysmon_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_sysmon_panel_parent_class)->finalize (object);
}

static void
gb_sysmon_panel_class_init (GbSysmonPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_sysmon_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/sysmon/gb-sysmon-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbSysmonPanel, cpu_graph);

  g_type_ensure (RG_TYPE_CPU_GRAPH);
}

static void
gb_sysmon_panel_init (GbSysmonPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
