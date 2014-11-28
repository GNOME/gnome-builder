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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "gb-application.h"
#include "gb-editor-file-marks.h"
#include "gb-editor-document.h"
#include "gb-editor-frame.h"
#include "gb-editor-workspace.h"
#include "gb-log.h"
#include "gb-keybindings.h"
#include "gb-preferences-window.h"
#include "gb-resources.h"
#include "gb-workbench.h"

#define ADWAITA_CSS  "resource:///org/gnome/builder/css/builder.Adwaita.css"
#define LANGUAGE_SCHEMA "org.gnome.builder.editor.language"
#define LANGUAGE_PATH "/org/gnome/builder/editor/language/"
#define GSV_PATH "resource:///org/gnome/builder/styles/"

G_DEFINE_TYPE (GbApplication, gb_application, GTK_TYPE_APPLICATION)

static void
gb_application_setup_search_paths (void)
{
  GtkSourceStyleSchemeManager *mgr;

  mgr = gtk_source_style_scheme_manager_get_default ();
  gtk_source_style_scheme_manager_append_search_path (
      mgr, PACKAGE_DATADIR"/gtksourceview-3.0/styles/");
}

static void
gb_application_install_language_defaults (GbApplication *self)
{
  gchar *defaults_installed_path;
  gboolean exists;

  g_return_if_fail (GB_IS_APPLICATION (self));

  defaults_installed_path = g_build_filename (g_get_user_data_dir (),
                                              "gnome-builder",
                                              ".defaults-installed",
                                              NULL);
  exists = g_file_test (defaults_installed_path, G_FILE_TEST_EXISTS);

  if (!exists)
    {
      GKeyFile *key_file;
      GBytes *bytes;

      key_file = g_key_file_new ();

      bytes =
        g_resources_lookup_data ("/org/gnome/builder/language/defaults.ini",
                                 0, NULL);

      if (bytes)
        {
          if (g_key_file_load_from_data (key_file,
                                         g_bytes_get_data (bytes, NULL),
                                         g_bytes_get_size (bytes),
                                         0, NULL))
            {
              gchar **groups;
              guint i;

              groups = g_key_file_get_groups (key_file, NULL);

              for (i = 0; groups [i]; i++)
                {
                  GSettings *settings;
                  gchar *settings_path;
                  gchar **keys;
                  guint j;

                  settings_path = g_strdup_printf (
                      "/org/gnome/builder/editor/language/%s/", groups [i]);
                  settings = g_settings_new_with_path (
                      "org.gnome.builder.editor.language",
                      settings_path);
                  g_free (settings_path);

                  keys = g_key_file_get_keys (key_file, groups [i], NULL, NULL);

                  for (j = 0; keys [j]; j++)
                    {
                      GVariant *param;
                      gchar *value;

                      value = g_key_file_get_value (key_file, groups [i],
                                                    keys [j], NULL);
                      param = g_variant_parse (NULL, value, NULL, NULL, NULL);

                      if (param)
                        {
                          g_settings_set_value (settings, keys [j], param);
                          g_variant_unref (param);
                        }

                      g_free (value);
                    }

                  g_object_unref (settings);
                  g_strfreev (keys);
                }

              g_strfreev (groups);
            }

          g_bytes_unref (bytes);
        }

      g_key_file_free (key_file);
      g_file_set_contents (defaults_installed_path, "", 0, NULL);
    }

  g_free (defaults_installed_path);
}

static void
gb_application_make_skeleton_dirs (GbApplication *self)
{
  gchar *path;

  g_return_if_fail (GB_IS_APPLICATION (self));

  path = g_build_filename (g_get_user_data_dir (),
                           "gnome-builder",
                           NULL);
  g_mkdir_with_parents (path, 0750);
  g_free (path);

  path = g_build_filename (g_get_user_config_dir (),
                           "gnome-builder",
                           NULL);
  g_mkdir_with_parents (path, 0750);
  g_free (path);

  path = g_build_filename (g_get_user_config_dir (),
                           "gnome-builder",
                           "snippets",
                           NULL);
  g_mkdir_with_parents (path, 0750);
  g_free (path);

  path = g_build_filename (g_get_user_config_dir (),
                           "gnome-builder",
                           "uncrustify",
                           NULL);
  g_mkdir_with_parents (path, 0750);
  g_free (path);
}

static void
gb_application_load_file_marks (GbApplication *application)
{
  GbEditorFileMarks *marks;
  GError *error = NULL;

  g_return_if_fail (GB_IS_APPLICATION (application));

  marks = gb_editor_file_marks_get_default ();

  if (!gb_editor_file_marks_load (marks, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
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
      g_warning (_("Failed to load default keybindings: %s"), error->message);
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
      g_warning (_("Failed to load local keybindings: %s"), error->message);
      g_clear_error (&error);
    }
  g_free (path);

  gb_keybindings_register (keybindings, GTK_APPLICATION (self));

  g_object_unref (keybindings);

  EXIT;
}

static GbWorkbench *
gb_application_create_workbench (GApplication *application)
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

  gtk_application_add_window (GTK_APPLICATION (application), window);

  RETURN (GB_WORKBENCH (window));
}

static void
gb_application_activate (GApplication *application)
{
  GbWorkbench *workbench;
  GList *list;

  {
    GbEditorDocument *document;
    GtkSourceFile *file;
    GFile *gfile;
    GtkWindow *window;
    GtkWidget *frame;

    file = gtk_source_file_new ();
    gfile = g_file_new_for_path ("src/app/gb-application.c");
    gtk_source_file_set_location (file, gfile);
    g_object_unref (gfile);

    document = g_object_new (GB_TYPE_EDITOR_DOCUMENT, "file", file, NULL);
    g_object_unref (file);

    window = g_object_new (GTK_TYPE_WINDOW, NULL);
    frame = g_object_new (GB_TYPE_EDITOR_FRAME,
                          "document", document,
                          "visible", TRUE,
                          NULL);
    gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (frame));
    gtk_window_present (window);
  }

  g_return_if_fail (GB_IS_APPLICATION (application));

  list = gtk_application_get_windows (GTK_APPLICATION (application));

  for (; list; list = list->next)
    {
      if (GB_IS_WORKBENCH (list->data))
        {
          gtk_window_present (GTK_WINDOW (list->data));
          return;
        }
    }

  workbench = gb_application_create_workbench (application);

  gtk_window_present (GTK_WINDOW (workbench));
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
    workbench = GB_WORKBENCH (gb_application_create_workbench (application));

  gtk_window_present (GTK_WINDOW (workbench));

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
gb_application_activate_preferences_action (GSimpleAction *action,
                                            GVariant      *parameter,
                                            gpointer       user_data)
{
  GbApplication *application = user_data;
  GbPreferencesWindow *window;
  GbWorkbench *workbench = NULL;
  GList *list;

  g_return_if_fail (GB_IS_APPLICATION (application));

  list = gtk_application_get_windows (GTK_APPLICATION (application));

  for (; list; list = list->next)
    if (GB_IS_WORKBENCH (list->data))
      workbench = GB_WORKBENCH (list->data);

  window = g_object_new (GB_TYPE_PREFERENCES_WINDOW,
                         "transient-for", workbench,
                         NULL);

  gtk_window_present (GTK_WINDOW (window));
}

static void
gb_application_activate_about_action (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       user_data)
{
  GList *list;

  g_return_if_fail (GB_IS_APPLICATION (user_data));

  list = gtk_application_get_windows (GTK_APPLICATION (user_data));

  for (; list; list = list->next)
    {
      if (GB_IS_WORKBENCH (list->data))
        {
          gb_workbench_roll_credits (list->data);
          gtk_window_present (list->data);
          break;
        }
    }
}

static void
gb_application_register_actions (GbApplication *self)
{
  static const GActionEntry action_entries[] = {
    { "about", gb_application_activate_about_action },
    { "preferences", gb_application_activate_preferences_action },
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
  GbApplication *self = (GbApplication *)app;

  ENTRY;

  g_assert (GB_IS_APPLICATION (self));

  g_resources_register (gb_get_resource ());
  g_application_set_resource_base_path (app, "/org/gnome/builder");

  G_APPLICATION_CLASS (gb_application_parent_class)->startup (app);

  gb_application_make_skeleton_dirs (self);
  gb_application_install_language_defaults (self);
  gb_application_register_actions (self);
  gb_application_register_keybindings (self);
  gb_application_register_theme_overrides (self);
  gb_application_load_file_marks (self);
  gb_application_setup_search_paths ();

  EXIT;
}

static void
gb_application_shutdown (GApplication *app)
{
  GbApplication *self = (GbApplication *)app;
  GbEditorFileMarks *marks;
  GError *error = NULL;

  ENTRY;

  g_assert (GB_IS_APPLICATION (self));

  marks = gb_editor_file_marks_get_default ();

  if (!gb_editor_file_marks_save (marks, NULL, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  G_APPLICATION_CLASS (gb_application_parent_class)->shutdown (app);

  EXIT;
}

static void
gb_application_constructed (GObject *object)
{
  ENTRY;

  if (G_OBJECT_CLASS (gb_application_parent_class)->constructed)
    G_OBJECT_CLASS (gb_application_parent_class)->constructed (object);

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
  app_class->shutdown = gb_application_shutdown;
  app_class->open = gb_application_open;

  EXIT;
}

static void
gb_application_init (GbApplication *application)
{
  ENTRY;
  EXIT;
}
