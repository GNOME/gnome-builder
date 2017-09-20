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
#include <gtksourceview/gtksource.h>

#include "application/ide-application-private.h"

#include "bug-buddy.h"

static IdeApplicationMode early_mode;

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
early_params_check (gint    *argc,
                    gchar ***argv)
{
  g_autofree gchar *type = NULL;
  g_autoptr(GOptionContext) context = NULL;
  GOptionEntry entries[] = {
    { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, verbose_cb },
    { "type", 0, 0, G_OPTION_ARG_STRING, &type },
    { NULL }
  };

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_parse (context, argc, argv, NULL);

  if (g_strcmp0 (type, "worker") == 0)
    early_mode = IDE_APPLICATION_MODE_WORKER;
  else if (g_strcmp0 (type, "cli") == 0)
    early_mode = IDE_APPLICATION_MODE_TOOL;
}

static void
early_ssl_check (void)
{
  /*
   * This tries to locate the SSL cert.pem and overrides the environment
   * variable. Otherwise, chances are we won't be able to validate SSL
   * certificates while inside of flatpak.
   *
   * Ideally, we will be able to delete this once Flatpak has a solution
   * for SSL certificate management inside of applications.
   */
  if (ide_is_flatpak ())
    {
      if (NULL == g_getenv ("SSL_CERT_FILE"))
        {
          static const gchar *ssl_cert_paths[] = {
            "/etc/pki/tls/cert.pem",
            "/etc/ssl/cert.pem",
            NULL
          };

          for (guint i = 0; ssl_cert_paths[i]; i++)
            {
              if (g_file_test (ssl_cert_paths[i], G_FILE_TEST_EXISTS))
                {
                  g_setenv ("SSL_CERT_FILE", ssl_cert_paths[i], TRUE);
                  g_message ("Using “%s” for SSL_CERT_FILE.", ssl_cert_paths[i]);
                  break;
                }
            }
        }
    }
}

int
main (int   argc,
      char *argv[])
{
  IdeApplication *app;
  int ret;

  bug_buddy_init ();

  ide_log_init (TRUE, NULL);
  early_params_check (&argc, &argv);

  early_ssl_check ();

  g_message ("Initializing with Gtk+ version %d.%d.%d.",
             gtk_get_major_version (),
             gtk_get_minor_version (),
             gtk_get_micro_version ());

  /*
   * FIXME: Work around type registration deadlocks in GObject.
   *
   * There seems to be a deadlock in GObject type registration from
   * different threads based on some different parts of the type system.
   * This is problematic because we occasionally race during startup in
   * GZlibDecopmressor's get_type() registration.
   *
   * Instead, we'll just deal with this early by registering the type
   * as soon as possible.
   *
   * https://bugzilla.gnome.org/show_bug.cgi?id=779199
   */
  g_type_ensure (G_TYPE_ZLIB_DECOMPRESSOR);

  app = ide_application_new ();
  _ide_application_set_mode (app, early_mode);
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  g_clear_object (&app);

  ide_log_shutdown ();

  return ret;
}
