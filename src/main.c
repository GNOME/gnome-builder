/* main.c
 *
 * Copyright 2018-2023 Christian Hergert <chergert@redhat.com>
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

#include <locale.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib/gi18n.h>

#include <girepository/girepository.h>
#include <gtksourceview/gtksource.h>

#ifdef ENABLE_TRACING_SYSCAP
# include <sysprof-capture.h>
#endif

#include <libide-core.h>
#include <libide-code.h>
#include <libide-gui.h>
#include <libide-threading.h>

#include "ide-application-private.h"
#include "ide-build-ident.h"
#include "ide-editor-private.h"
#include "ide-gtk-private.h"
#include "ide-log-private.h"
#include "ide-search-private.h"
#include "ide-shell-private.h"
#include "ide-terminal-private.h"
#include "ide-thread-private.h"
#include "ide-tweaks-init.h"
#include "ide-private.h"

#include "bug-buddy.h"

#ifdef ENABLE_TRACING_SYSCAP
static inline int
current_cpu (void)
{
#ifdef HAVE_SCHED_GETCPU
  return sched_getcpu ();
#else
  return 0;
#endif
}

static void
trace_load (void)
{
  sysprof_clock_init ();
  sysprof_collector_init ();
}

static void
trace_unload (void)
{
}

static void
trace_function (const gchar    *func,
                gint64          begin_time_usec,
                gint64          end_time_usec)
{
  sysprof_collector_mark (begin_time_usec * 1000L,
                          (end_time_usec - begin_time_usec) * 1000L,
                          "tracing",
                          "call",
                          func);
}

static void
trace_log (GLogLevelFlags  log_level,
           const gchar    *domain,
           const gchar    *message)
{
  sysprof_collector_log (log_level, domain, message);
}

static IdeTraceVTable trace_vtable = {
  trace_load,
  trace_unload,
  trace_function,
  trace_log,
};
#endif

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
                    gboolean   *version)
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GOptionGroup) gir_group = NULL;
  GOptionEntry entries[] = {
    { "standalone", 's', 0, G_OPTION_ARG_NONE, standalone, N_("Run a new instance of Builder") },
    { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, verbose_cb },
    { "version", 'V', 0, G_OPTION_ARG_NONE, version },
    { NULL }
  };

  gir_group = gi_repository_get_option_group ();

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, g_steal_pointer (&gir_group));
  g_option_context_parse (context, argc, argv, NULL);
}

static gboolean
_home_contains_symlink (const gchar *path)
{
  g_autofree gchar *parent = NULL;

  if (g_file_test (path, G_FILE_TEST_IS_SYMLINK))
    return TRUE;

  if ((parent = g_path_get_dirname (path)) && !g_str_equal (parent, "/"))
    return _home_contains_symlink (parent);

  return FALSE;
}

static gboolean
home_contains_symlink (void)
{
  return _home_contains_symlink (g_get_home_dir ());
}

static gboolean
is_running_in_shell (void)
{
  const gchar *shlvl = g_getenv ("SHLVL");

  /* GNOME Shell, among other desktop shells may set SHLVL=0 to indicate
   * that we are not running within a shell. Use that before checking any
   * file-descriptors since it is more reliable.
   */
  if (ide_str_equal0 (shlvl, "0"))
    return FALSE;

  /* If stdin is not a TTY, then assume we have no access to communicate
   * with the user via console. We use stdin instead of stdout as a logging
   * system may have a PTY for stdout to get colorized output.
   */
  if (!isatty (STDIN_FILENO))
    return FALSE;

  return TRUE;
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autofree char *messages_debug = NULL;
  IdeApplication *app;
  const gchar *desktop;
  gboolean standalone = FALSE;
  gboolean version = FALSE;
  int ret;

  /* Get environment variable early and clear it from GLib. We want to be
   * certain we don't pass this on to child processes so we clear it upfront.
   */
  messages_debug = g_strdup (getenv ("G_MESSAGES_DEBUG"));
  unsetenv ("G_MESSAGES_DEBUG");

  /* Setup our gdb fork()/exec() helper if we're in a terminal */
  if (is_running_in_shell ())
    bug_buddy_init ();

  /* Always ignore SIGPIPE */
  signal (SIGPIPE, SIG_IGN);

  /* Set up gettext translations */
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Setup various application name/id defaults. */
  g_set_prgname (ide_get_program_name ());
  g_set_application_name (_("Builder"));

#ifdef DEVELOPMENT_BUILD
  ide_set_application_id ("org.gnome.Builder.Devel");
#endif

  /* Early init of logging so that we get messages in a consistent
   * format. If we deferred this to GApplication, we'd get them in
   * multiple formats.
   */
  ide_log_init (TRUE, NULL, messages_debug);

  /* Extract options like -vvvv */
  early_params_check (&argc, &argv, &standalone, &version);

  /* Log some info so it shows up in logs */
  g_message ("GNOME Builder %s (%s) from channel \"%s\" starting with ABI %s",
             PACKAGE_VERSION, IDE_BUILD_IDENTIFIER, IDE_BUILD_CHANNEL, PACKAGE_ABI_S);

  if (version)
    {
#ifdef DEVELOPMENT_BUILD
      g_print ("GNOME Builder %s (%s)\n", PACKAGE_VERSION, IDE_BUILD_IDENTIFIER);
#else
      g_print  ("GNOME Builder "PACKAGE_VERSION"\n");
#endif
      return EXIT_SUCCESS;
    }

  /* Make sure $HOME is not a symlink, as that can cause issues with
   * various subsystems. Just warn super loud so that users find it
   * when trying to debug issues.
   *
   * Silverblue did this, but has since stopped (and some users will
   * lag behind until their systems are fixed).
   *
   * https://gitlab.gnome.org/GNOME/gnome-builder/issues/859
   */
  if (home_contains_symlink ())
    g_critical ("User home directory uses a symlink. "
                "This is not supported and may result in unforeseen issues.");

  /* Log what desktop is being used to simplify tracking down
   * quirks in the future.
   */
  desktop = g_getenv ("XDG_CURRENT_DESKTOP");
  if (desktop == NULL)
    desktop = "unknown";

#ifdef ENABLE_TRACING_SYSCAP
  _ide_trace_init (&trace_vtable);
#endif

  g_message ("Initializing with %s desktop and GTK+ %d.%d.%d.",
             desktop,
             gtk_get_major_version (),
             gtk_get_minor_version (),
             gtk_get_micro_version ());

  /* Init libraries with initializers */
  gtk_init ();
  gtk_source_init ();
  adw_init ();
  panel_init ();

  /* Initialize thread pools */
  _ide_thread_pool_init (FALSE);

  /* Guess the user $SHELL and $PATH early */
  _ide_shell_init ();

  /* Ensure availability of some symbols possibly dropped in link */
  _ide_tweaks_init ();
  _ide_gtk_init ();
  _ide_search_init ();
  _ide_editor_init ();
  _ide_terminal_init ();

  app = _ide_application_new (standalone);
  g_application_add_option_group (G_APPLICATION (app), gi_repository_get_option_group ());
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  /* Force disposal of the application (to help catch cleanup
   * issues at shutdown) and then (hopefully) finalize the app.
   */
  g_object_run_dispose (G_OBJECT (app));
  g_clear_object (&app);

  /* Flush any outstanding logs */
  ide_log_shutdown ();

  /* Cleanup GtkSourceView singletons to improve valgrind output */
  gtk_source_finalize ();

#ifdef ENABLE_TRACING_SYSCAP
  _ide_trace_shutdown ();
#endif

  return ret;
}
