/* gb-terminal-application-addin.c
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

#include "gb-terminal-application-addin.h"

struct _GbTerminalApplicationAddin
{
  GObject parent_instance;
};

static void application_addin_iface_init (GbApplicationAddinInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GbTerminalApplicationAddin,
                                gb_terminal_application_addin,
                                G_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (GB_TYPE_APPLICATION_ADDIN,
                                                       application_addin_iface_init))

static void
gb_terminal_application_addin_load (GbApplicationAddin *addin,
                                    GbApplication      *application)
{
  const gchar *new_terminal_accels[] = { "<ctrl><shift>t", NULL };

  g_assert (GB_IS_TERMINAL_APPLICATION_ADDIN (addin));
  g_assert (GB_IS_APPLICATION (application));

  gtk_application_set_accels_for_action (GTK_APPLICATION (application), "win.new-terminal",
                                         new_terminal_accels);
}

static void
gb_terminal_application_addin_unload (GbApplicationAddin *addin,
                                      GbApplication      *application)
{
  g_assert (GB_IS_TERMINAL_APPLICATION_ADDIN (addin));
  g_assert (GB_IS_APPLICATION (application));

  gtk_application_set_accels_for_action (GTK_APPLICATION (application), "win.new-terminal", NULL);
}

static void
gb_terminal_application_addin_class_init (GbTerminalApplicationAddinClass *klass)
{
}

static void
gb_terminal_application_addin_class_finalize (GbTerminalApplicationAddinClass *klass)
{
}

static void
gb_terminal_application_addin_init (GbTerminalApplicationAddin *self)
{
}

static void
application_addin_iface_init (GbApplicationAddinInterface *iface)
{
  iface->load = gb_terminal_application_addin_load;
  iface->unload = gb_terminal_application_addin_unload;
}

void
_gb_terminal_application_addin_register_type (GTypeModule *module)
{
  gb_terminal_application_addin_register_type (module);
}
