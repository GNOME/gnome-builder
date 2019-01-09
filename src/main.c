/* main.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "main"

#include "config.h"

#include <girepository.h>
#include <glib/gi18n.h>
#include <libide-core.h>
#include <libide-code.h>
#include <libide-editor.h>
#include <libide-greeter.h>
#include <libide-gui.h>
#include <libide-threading.h>

#include "ide-application-private.h"
#include "ide-thread-private.h"
#include "ide-terminal-private.h"

#include "bug-buddy.h"

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
early_params_check (gint       *argc,
                    gchar    ***argv,
                    gboolean   *standalone,
                    gchar     **type,
                    gchar     **plugin,
                    gchar     **dbus_address)
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GOptionGroup) gir_group = NULL;
  GOptionEntry entries[] = {
    { "standalone", 's', 0, G_OPTION_ARG_NONE, standalone, N_("Run a new instance of Builder") },
    { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, verbose_cb },
    { "plugin", 0, 0, G_OPTION_ARG_STRING, plugin },
    { "type", 0, 0, G_OPTION_ARG_STRING, type },
    { "dbus-address", 0, 0, G_OPTION_ARG_STRING, dbus_address },
    { NULL }
  };

  gir_group = g_irepository_get_option_group ();

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gir_group);
  g_option_context_parse (context, argc, argv, NULL);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autofree gchar *plugin = NULL;
  g_autofree gchar *type = NULL;
  g_autofree gchar *dbus_address = NULL;
  IdeApplication *app;
  const gchar *desktop;
  gboolean standalone = FALSE;
  int ret;

  /* Setup our gdb fork()/exec() helper */
  bug_buddy_init ();

  /* Always ignore SIGPIPE */
  signal (SIGPIPE, SIG_IGN);

  /* Setup various application name/id defaults. */
  g_set_prgname (ide_get_program_name ());
  g_set_application_name (_("Builder"));

#if 0
  /* TODO: allow support for parallel nightly install */
#ifdef DEVELOPMENT_BUILD
  ide_set_application_id ("org.gnome.Builder-Devel");
#endif
#endif

  /* Early init of logging so that we get messages in a consistent
   * format. If we deferred this to GApplication, we'd get them in
   * multiple formats.
   */
  ide_log_init (TRUE, NULL);

  /* Extract options like -vvvv */
  early_params_check (&argc, &argv, &standalone, &type, &plugin, &dbus_address);

  /* Log what desktop is being used to simplify tracking down
   * quirks in the future.
   */
  desktop = g_getenv ("XDG_CURRENT_DESKTOP");
  if (desktop == NULL)
    desktop = "unknown";

  g_message ("Initializing with %s desktop and GTK+ %d.%d.%d.",
             desktop,
             gtk_get_major_version (),
             gtk_get_minor_version (),
             gtk_get_micro_version ());

  /* Initialize thread pools */
  _ide_thread_pool_init (FALSE);

  /* Guess the user shell early */
  _ide_guess_shell ();

  app = _ide_application_new (standalone, type, plugin, dbus_address);
  g_application_add_option_group (G_APPLICATION (app), g_irepository_get_option_group ());
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  /* Force disposal of the application (to help catch cleanup
   * issues at shutdown) and then (hopefully) finalize the app.
   */
  g_object_run_dispose (G_OBJECT (app));
  g_clear_object (&app);

  /* Flush any outstanding logs */
  ide_log_shutdown ();

  return ret;
}
