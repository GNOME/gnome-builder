/* gb-application.c
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

#define G_LOG_DOMAIN "app"
#define ADWAITA_CSS "resource:///org/gnome/builder/css/builder.Adwaita.css"

#include <glib/gi18n.h>

#include "gb-application.h"
#include "gb-log.h"
#include "gb-keybindings.h"
#include "gb-resources.h"
#include "gb-workbench.h"

G_DEFINE_TYPE (GbApplication, gb_application, GTK_TYPE_APPLICATION)

static void
theme_changed (GtkSettings *settings)
{
  static GtkCssProvider *provider = NULL;
  GdkScreen *screen;
  gchar *theme;

  ENTRY;

  g_object_get (settings, "gtk-theme-name", &theme, NULL);
  screen = gdk_screen_get_default ();

  if (g_str_equal (theme, "Adwaita"))
    {
      if (provider == NULL)
        {
          GFile *file;

          provider = gtk_css_provider_new ();
          file = g_file_new_for_uri (ADWAITA_CSS);
          gtk_css_provider_load_from_file (provider, file, NULL);
          g_object_unref (file);
        }

      gtk_style_context_add_provider_for_screen (screen,
                                                 GTK_STYLE_PROVIDER (provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
  else if (provider != NULL)
    {
      gtk_style_context_remove_provider_for_screen (screen,
                                                    GTK_STYLE_PROVIDER (provider));
      g_clear_object (&provider);
    }

  g_free (theme);

  EXIT;
}

static void
setup_theme_extensions (void)
{
  GtkSettings *settings;

  ENTRY;

  /* Set up a handler to load our custom css for Adwaita.
   * See https://bugzilla.gnome.org/show_bug.cgi?id=732959
   * for a more automatic solution that is still under discussion.
   */
  settings = gtk_settings_get_default ();
  g_signal_connect (settings,
                    "notify::gtk-theme-name",
                    G_CALLBACK (theme_changed),
                    NULL);
  theme_changed (settings);

  EXIT;
}

static void
setup_keybindings (GbApplication *application)
{
  GbKeybindings *keybindings;
  GError *error = NULL;
  GBytes *bytes;
  gchar *path;

  ENTRY;

  g_assert (GB_IS_APPLICATION (application));

  keybindings = gb_keybindings_new ();

  /*
   * Load bundled keybindings.
   */
  bytes = g_resources_lookup_data ("/org/gnome/builder/keybindings/default.ini",
                                   G_RESOURCE_FLAGS_NONE, NULL);
  if (!gb_keybindings_load_bytes (keybindings, bytes, &error))
    {
      g_warning ("Failed to load default keybindings: %s", error->message);
      g_clear_error (&error);
    }
  g_bytes_unref (bytes);

  /*
   * Load local overrides from ~/.config/gnome-builder/keybindings.ini
   */
  path = g_build_filename (g_get_user_config_dir (),
                           "gnome-builder",
                           "keybindings.ini",
                           NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS) &&
      !gb_keybindings_load_path (keybindings, path, &error))
    {
      g_warning ("Failed to load local keybindings: %s", error->message);
      g_clear_error (&error);
    }
  g_free (path);

  gb_keybindings_register (keybindings, GTK_APPLICATION (application));

  g_object_unref (keybindings);

  EXIT;
}

static void
gb_application_activate (GApplication *application)
{
  GtkWindow *window;
  GdkScreen *screen;
  GdkRectangle geom;
  gint primary;
  gint default_width;
  gint default_height;

  ENTRY;

  screen = gdk_screen_get_default ();
  primary = gdk_screen_get_primary_monitor (screen);
  gdk_screen_get_monitor_geometry (screen, primary, &geom);
  default_width = (geom.width / 4) * 3;
  default_height = (geom.height / 4) * 3;

  window = g_object_new (GB_TYPE_WORKBENCH,
                         "title", _ ("Builder"),
                         "default-width", default_width,
                         "default-height", default_height,
                         "window-position", GTK_WIN_POS_CENTER,
                         NULL);

  gtk_application_add_window (GTK_APPLICATION (application), window);

  gtk_window_present (window);

  EXIT;
}

static void
gb_application_startup (GApplication *app)
{
  ENTRY;

  G_APPLICATION_CLASS (gb_application_parent_class)->startup (app);

  g_resources_register (gb_get_resource ());
  setup_theme_extensions ();
  setup_keybindings (GB_APPLICATION (app));

  EXIT;
}

static void
gb_application_class_init (GbApplicationClass *klass)
{
  GApplicationClass *app_class;

  ENTRY;

  app_class = G_APPLICATION_CLASS (klass);
  app_class->activate = gb_application_activate;
  app_class->startup = gb_application_startup;

  EXIT;
}

static void
gb_application_init (GbApplication *application)
{
  ENTRY;
  EXIT;
}
