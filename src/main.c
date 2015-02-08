/* main.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "Builder"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <ide.h>
#include <locale.h>

#include "gb-application.h"
#include "gb-log.h"

int
main (int   argc,
      char *argv[])
{
  GApplication *app;
  int ret;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_set_prgname (PACKAGE_TARNAME);
  g_set_application_name (_("Builder"));

  ide_set_program_name ("gnome-builder");

  gb_log_init (TRUE, NULL);

  g_message ("Initializing with Gtk+ version %d.%d.%d.",
             gtk_get_major_version (),
             gtk_get_minor_version (),
             gtk_get_micro_version ());

  app = g_object_new (GB_TYPE_APPLICATION,
                      "application-id", "org.gnome.Builder",
                      "flags", G_APPLICATION_HANDLES_OPEN,
                      NULL);
  g_application_set_default (app);
  ret = g_application_run (app, argc, argv);
  g_clear_object (&app);

  gb_log_shutdown ();

  return ret;
}
