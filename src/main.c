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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gb-application.h"
#include "gb-log.h"

int
main (int   argc,
      char *argv[])
{
  GApplication *app;
  int ret;

  g_set_prgname ("gnome-builder");
  g_set_application_name (_ ("Builder"));

  gb_log_init (TRUE, NULL);

  app = g_object_new (GB_TYPE_APPLICATION,
                      "application-id", "org.gnome.Builder",
                      NULL);
  g_application_set_default (app);

  ret = g_application_run (app, argc, argv);

  g_clear_object (&app);

  gb_log_shutdown ();

  return ret;
}
