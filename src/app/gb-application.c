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
#define ADWAITA_CSS  "resource:///org/gnome/builder/css/builder.Adwaita.css"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "gb-application.h"
#include "gb-editor-workspace.h"
#include "gb-log.h"
#include "gb-keybindings.h"
#include "gb-resources.h"
#include "gb-workbench.h"

G_DEFINE_TYPE (GbApplication, gb_application, GTK_TYPE_APPLICATION)

GbApplication *
gb_application_new (void)
{
  return g_object_new (GB_TYPE_APPLICATION, NULL);
}

static void
gb_application_on_theme_changed (GbApplication *self,
                                 GParamSpec    *pspec,
                                 GtkSettings   *settings)
{
  static GtkCssProvider *provider = NULL;
  GdkScreen *screen;
  gchar *theme;

  ENTRY;

  g_assert (GB_IS_APPLICATION (self));
  g_assert (GTK_IS_SETTINGS (settings));

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
gb_application_register_theme_overrides (GbApplication *application)
{
  GtkSettings *settings;

  ENTRY;

  /* Set up a handler to load our custom css for Adwaita.
   * See https://bugzilla.gnome.org/show_bug.cgi?id=732959
   * for a more automatic solution that is still under discussion.
   */
  settings = gtk_settings_get_default ();
  g_signal_connect_object (settings,
                           "notify::gtk-theme-name",
                           G_CALLBACK (gb_application_on_theme_changed),
                           application,
                           G_CONNECT_SWAPPED);
  gb_application_on_theme_changed (application, NULL, settings);

  EXIT;
}

static void
gb_application_register_keybindings (GbApplication *self)
{
  GbKeybindings *keybindings;
  GError *error = NULL;
  GBytes *bytes;
  gchar *path;

  ENTRY;

  g_assert (GB_IS_APPLICATION (self));

  keybindings = gb_keybindings_new ();

  /*
   * Load bundled keybindings.
   */
  bytes = g_resources_lookup_data ("/org/gnome/builder/keybindings/default.ini",
                                   G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
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

  gb_keybindings_register (keybindings, GTK_APPLICATION (self));

  g_object_unref (keybindings);

  EXIT;
}

static GtkWindow *
create_window (GApplication *application)
{
  GtkWindow *window;
  GdkScreen *screen;
  GdkRectangle geom;
  gint primary;
  gint default_width;
  gint default_height;

  ENTRY;

  /*
   * Determine 3/4's the screen width for the default size. We will maximize
   * the window anyway, but handy when unmaximizing.
   */
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

  gtk_window_maximize (window);
  gtk_window_present (window);

  gtk_application_add_window (GTK_APPLICATION (application), window);

  RETURN (window);
}

static void
gb_application_activate (GApplication *application)
{
  create_window (application);
}

static void
gb_application_open (GApplication   *application,
                     GFile         **files,
                     gint            n_files,
                     const gchar    *hint)
{
  GbWorkbench *workbench = NULL;
  GbWorkspace *workspace;
  GList *list;
  guint i;

  g_assert (GB_IS_APPLICATION (application));

  list = gtk_application_get_windows (GTK_APPLICATION (application));

  for (; list; list = list->next)
    {
      if (GB_IS_WORKBENCH (list->data))
        {
          workbench = GB_WORKBENCH (list->data);
          break;
        }
    }

  if (!workbench)
    workbench = GB_WORKBENCH (create_window (application));

  workspace = gb_workbench_get_workspace (workbench,
                                          GB_TYPE_EDITOR_WORKSPACE);

  g_assert (GB_IS_EDITOR_WORKSPACE (workspace));

  for (i = 0; i < n_files; i++)
    {
      g_return_if_fail (G_IS_FILE (files [i]));
      gb_editor_workspace_open (GB_EDITOR_WORKSPACE (workspace), files [i]);
    }
}

static void
gb_application_activate_quit_action (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  g_return_if_fail (GB_IS_APPLICATION (user_data));

  g_application_quit (G_APPLICATION (user_data));
}

static void
gb_application_activate_about_action (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       user_data)
{
  GtkWindow *window;
  GList *list;
  GBytes *bytes;
  gchar **authors;
  gchar **artists;
  gchar *translators;

  g_return_if_fail (GB_IS_APPLICATION (user_data));

  list = gtk_application_get_windows (GTK_APPLICATION (user_data));

  bytes = g_resources_lookup_data ("/org/gnome/builder/AUTHORS", 0, NULL);
  authors = g_strsplit (g_bytes_get_data (bytes, NULL), "\n", 0);
  g_bytes_unref (bytes);

  bytes = g_resources_lookup_data ("/org/gnome/builder/ARTISTS", 0, NULL);
  artists = g_strsplit (g_bytes_get_data (bytes, NULL), "\n", 0);
  g_bytes_unref (bytes);

  bytes = g_resources_lookup_data ("/org/gnome/builder/TRANSLATORS", 0, NULL);
  translators = g_strdup (g_bytes_get_data (bytes, NULL));
  g_bytes_unref (bytes);

  window = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                         "artists", artists,
                         "authors", authors,
                         "comments", _("Builder is an IDE for writing GNOME applications."),
                         "copyright", "Copyright © 2014 Christian Hergert",
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", "builder",
                         "modal", TRUE,
                         "program-name", _("GNOME Builder"),
                         "transient-for", list ? list->data : NULL,
                         "translator-credits", translators,
                         "version", PACKAGE_VERSION,
                         "website", "https://live.gnome.org/Apps/Builder",
                         "website-label", _("Builder Website"),
                         "window-position", GTK_WIN_POS_CENTER,
                         NULL);

  gtk_window_present (window);

  g_strfreev (authors);
  g_strfreev (artists);
  g_free (translators);
}

static void
gb_application_register_actions (GbApplication *self)
{
  static const GActionEntry action_entries[] = {
    { "about", gb_application_activate_about_action },
    { "quit", gb_application_activate_quit_action },
  };

  g_return_if_fail (GB_IS_APPLICATION (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   action_entries,
                                   G_N_ELEMENTS (action_entries),
                                   self);
}

static void
gb_application_startup (GApplication *app)
{
  GbApplication *self;

  ENTRY;

  self = GB_APPLICATION (app);

  g_resources_register (gb_get_resource ());

  G_APPLICATION_CLASS (gb_application_parent_class)->startup (app);

  gb_application_register_actions (self);
  gb_application_register_keybindings (self);
  gb_application_register_theme_overrides (self);

  EXIT;
}

static void
gb_application_constructed (GObject *object)
{
  ENTRY;

  if (G_OBJECT_CLASS (gb_application_parent_class)->constructed)
    G_OBJECT_CLASS (gb_application_parent_class)->constructed (object);

  g_application_set_resource_base_path (G_APPLICATION (object),
                                        "/org/gnome/builder");

  EXIT;
}

static void
gb_application_class_init (GbApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  ENTRY;

  object_class->constructed = gb_application_constructed;

  app_class->activate = gb_application_activate;
  app_class->startup = gb_application_startup;
  app_class->open = gb_application_open;

  EXIT;
}

static void
gb_application_init (GbApplication *application)
{
  ENTRY;

  g_application_set_application_id (G_APPLICATION (application),
                                    "org.gnome.Builder");
  g_application_set_flags (G_APPLICATION (application),
                           G_APPLICATION_HANDLES_OPEN);

  EXIT;
}
