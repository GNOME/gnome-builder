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

#define G_LOG_DOMAIN "builder"

#include <ide.h>

static gboolean
verbose_cb (const gchar  *option_name,
            const gchar  *value,
            gpointer      data,
            GError      **error)
{
  ide_log_increase_verbosity ();
  return TRUE;
}

static void
early_verbose_check (gint    *argc,
                     gchar ***argv)
{
  GOptionContext *context;
  static const GOptionEntry entries[] = {
    { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, verbose_cb },
    { NULL }
  };

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_parse (context, argc, argv, NULL);
  g_option_context_free (context);
}

int
main (int   argc,
      char *argv[])
{
  IdeApplication *app;
  int ret;

  ide_log_init (TRUE, NULL);

  early_verbose_check (&argc, &argv);

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
