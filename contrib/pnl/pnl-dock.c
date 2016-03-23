/* pnl-dock.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "pnl-dock.h"
#include "pnl-resources.h"

G_DEFINE_INTERFACE (PnlDock, pnl_dock, GTK_TYPE_CONTAINER)

static void
pnl_dock_default_init (PnlDockInterface *iface)
{
  GdkScreen *screen;

  g_resources_register (pnl_get_resource ());

  screen = gdk_screen_get_default ();

  if (screen != NULL)
    gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                                      "/org/gnome/panel-gtk/icons");

  g_object_interface_install_property (iface,
                                       g_param_spec_object ("manager",
                                                            "Manager",
                                                            "Manager",
                                                            PNL_TYPE_DOCK_MANAGER,
                                                            (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}
