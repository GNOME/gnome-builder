/* test-helper.c
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

#include <glib.h>
#include <gtk/gtk.h>

#include "gb-plugins.h"

gboolean
fatal_log_handler (const gchar    *log_domain,
                   GLogLevelFlags  log_level,
                   const gchar    *message,
                   gpointer        user_data)
{
  if (g_strcmp0 (log_domain, "Devhelp") == 0)
    return FALSE;
  return TRUE;
}

void
test_helper_init (gint    *argc,
                  gchar ***argv)
{
  gtk_init (argc, argv);
  g_test_init (argc, argv, NULL);
  gb_plugins_init ();
}

void
test_helper_begin_test (void)
{
  g_test_log_set_fatal_handler (fatal_log_handler, NULL);
}
