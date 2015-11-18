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

#include <ide.h>

int
main (int   argc,
      char *argv[])
{
  IdeApplication *app;
  int ret;

  ide_log_init (TRUE, NULL);

  g_message ("Initializing with Gtk+ version %d.%d.%d.",
             gtk_get_major_version (),
             gtk_get_minor_version (),
             gtk_get_micro_version ());

  app = ide_application_new ();
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  g_clear_object (&app);

  ide_log_shutdown ();

  return ret;
}
