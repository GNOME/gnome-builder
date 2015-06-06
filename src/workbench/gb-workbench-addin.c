/* gb-workbench-addin.c
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

#include "gb-workbench.h"
#include "gb-workbench-addin.h"

G_DEFINE_INTERFACE (GbWorkbenchAddin, gb_workbench_addin, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_WORKBENCH,
  LAST_PROP
};

enum {
  LOAD,
  UNLOAD,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

static void
gb_workbench_addin_default_init (GbWorkbenchAddinInterface *iface)
{
  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("The workbench window."),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface, gParamSpecs [PROP_WORKBENCH]);

  gSignals [LOAD] = g_signal_new ("load",
                                  G_TYPE_FROM_INTERFACE (iface),
                                  G_SIGNAL_RUN_LAST,
                                  G_STRUCT_OFFSET (GbWorkbenchAddinInterface, load),
                                  NULL, NULL, NULL, G_TYPE_NONE, 0);

  gSignals [UNLOAD] = g_signal_new ("unload",
                                    G_TYPE_FROM_INTERFACE (iface),
                                    G_SIGNAL_RUN_LAST,
                                    G_STRUCT_OFFSET (GbWorkbenchAddinInterface, unload),
                                    NULL, NULL, NULL, G_TYPE_NONE, 0);
}

void
gb_workbench_addin_load (GbWorkbenchAddin *self)
{
  g_return_if_fail (GB_IS_WORKBENCH_ADDIN (self));

  g_signal_emit (self, gSignals [LOAD], 0);
}
void
gb_workbench_addin_unload (GbWorkbenchAddin *self)
{
  g_return_if_fail (GB_IS_WORKBENCH_ADDIN (self));

  g_signal_emit (self, gSignals [UNLOAD], 0);
}
