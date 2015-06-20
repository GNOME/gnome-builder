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
#include <libpeas/peas.h>
#include <locale.h>

#include "gb-application.h"
#include "gb-icons-resources.h"

static void
load_plugins (void)
{
  PeasEngine *engine;
  const GList *list;

  engine = peas_engine_get_default ();

  if (g_getenv ("GB_IN_TREE_PLUGINS") != NULL)
    {
      GDir *dir;

      if ((dir = g_dir_open ("plugins", 0, NULL)))
        {
          const gchar *name;

          while ((name = g_dir_read_name (dir)))
            {
              gchar *path;

              path = g_build_filename ("plugins", name, NULL);
              peas_engine_prepend_search_path (engine, path, path);
              g_free (path);
            }

          g_dir_close (dir);
        }
    }
  else
    {
      peas_engine_prepend_search_path (engine,
                                       PACKAGE_LIBDIR"/gnome-builder/plugins",
                                       PACKAGE_DATADIR"/gnome-builder/plugins");
    }

  list = peas_engine_get_plugin_list (engine);

  for (; list; list = list->next)
    {
      if (peas_plugin_info_is_builtin (list->data))
        peas_engine_load_plugin (engine, list->data);
    }
}

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

  ide_log_init (TRUE, NULL);

  g_message ("Initializing with Gtk+ version %d.%d.%d.",
             gtk_get_major_version (),
             gtk_get_minor_version (),
             gtk_get_micro_version ());

  g_resources_register (gb_icons_get_resource ());
  load_plugins ();

  app = g_object_new (GB_TYPE_APPLICATION,
                      "application-id", "org.gnome.Builder",
                      "flags", G_APPLICATION_HANDLES_OPEN,
                      NULL);
  ret = g_application_run (app, argc, argv);
  g_clear_object (&app);

  ide_log_shutdown ();

  return ret;
}
