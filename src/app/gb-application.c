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

#define G_LOG_DOMAIN "gb-application"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <ide.h>

#include "gb-application.h"
#include "gb-application-actions.h"
#include "gb-application-private.h"
#include "gb-editor-document.h"
#include "gb-editor-workspace.h"
#include "gb-glib.h"
#include "gb-resources.h"
#include "gb-workbench.h"

#define ADWAITA_CSS "resource:///org/gnome/builder/css/builder.Adwaita.css"
#define GSV_PATH    "resource:///org/gnome/builder/styles/"

G_DEFINE_TYPE (GbApplication, gb_application, GTK_TYPE_APPLICATION)

static void
get_default_size (GtkRequisition *req)
{
  GdkScreen *screen;
  GdkRectangle rect;
  gint primary;

  screen = gdk_screen_get_default ();
  primary = gdk_screen_get_primary_monitor (screen);
  gdk_screen_get_monitor_geometry (screen, primary, &rect);

  req->width = rect.width * 0.75;
  req->height = rect.height * 0.75;
}

static void
gb_application_setup_search_paths (void)
{
  GtkSourceStyleSchemeManager *style_scheme_manager;
  static gboolean initialized;

  if (initialized)
    return;

  style_scheme_manager = gtk_source_style_scheme_manager_get_default ();
  gtk_source_style_scheme_manager_append_search_path (style_scheme_manager,
                                                      PACKAGE_DATADIR"/gtksourceview-3.0/styles/");
  initialized = TRUE;
}

/**
 * gb_application_make_skeleton_dirs:
 * @self: A #GbApplication.
 *
 * Creates all the directories we might need later. Simpler to just ensure they
 * are created during startup.
 */
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
                           "syntax",
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

/**
 * gb_application_on_theme_changed:
 * @self: A #GbApplication.
 *
 * Update the theme overrides when the theme changes. This includes our custom
 * CSS for Adwaita, etc.
 */
static void
gb_application_on_theme_changed (GbApplication *self,
                                 GParamSpec    *pspec,
                                 GtkSettings   *settings)
{
  static GtkCssProvider *provider;
  GdkScreen *screen;
  gchar *theme;

  IDE_ENTRY;

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

  IDE_EXIT;
}

static void
gb_application_register_theme_overrides (GbApplication *application)
{
  GtkSettings *settings;

  IDE_ENTRY;

  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                                    "/org/gnome/builder/icons/");

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

  IDE_EXIT;
}

static void
gb_application_load_keybindings (GbApplication *self)
{
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *name = NULL;
  static const struct { gchar *name; gchar *binding; } shared_bindings[] = {
    { "workbench.global-search", "<ctrl>period" },
    { "app.preferences", "<ctrl>comma" },
    { NULL }
  };
  gsize i;

  g_assert (GB_IS_APPLICATION (self));

  settings = g_settings_new ("org.gnome.builder.editor");
  name = g_settings_get_string (settings, "keybindings");
  self->keybindings = gb_keybindings_new (GTK_APPLICATION (self), name);
  g_settings_bind (settings, "keybindings", self->keybindings, "mode", G_SETTINGS_BIND_GET);

  for (i = 0; shared_bindings [i].name; i++)
    {
      const gchar *accels[2] = { shared_bindings [i].binding, NULL };
      gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                             shared_bindings [i].name,
                                             accels);
    }
}

static GbWorkbench *
gb_application_find_workbench_for_file (GbApplication *self,
                                        GFile         *file)
{
  GList *iter;
  GList *workbenches;

  g_assert (GB_IS_APPLICATION (self));
  g_assert (G_IS_FILE (file));

  workbenches = gtk_application_get_windows (GTK_APPLICATION (self));

  /*
   * Find the a project that contains this file in its working directory.
   */
  for (iter = workbenches; iter; iter = iter->next)
    {
      if (GB_IS_WORKBENCH (iter->data))
        {
          GbWorkbench *workbench = iter->data;
          g_autofree gchar *relpath = NULL;
          IdeContext *context;
          IdeVcs *vcs;
          GFile *workdir;

          context = gb_workbench_get_context (workbench);
          vcs = ide_context_get_vcs (context);
          workdir = ide_vcs_get_working_directory (vcs);

          relpath = g_file_get_relative_path (workdir, file);

          if (relpath != NULL)
            return workbench;
        }
    }

  /*
   * No matches found, take the first workbench we find.
   */
  for (iter = workbenches; iter; iter = iter->next)
    if (GB_IS_WORKBENCH (iter->data))
      return iter->data;

  return NULL;
}

static IdeBuffer *
on_create_buffer (IdeBufferManager *buffer_manager,
                  IdeFile          *file,
                  gpointer          user_data)
{
  return g_object_new (GB_TYPE_EDITOR_DOCUMENT,
                       "context", ide_object_get_context (IDE_OBJECT (buffer_manager)),
                       "file", file,
                       "highlight-diagnostics", TRUE,
                       NULL);
}

static void
gb_application__context_new_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeContext) context = NULL;
  IdeBufferManager *bufmgr;
  GbApplication *self;
  GbWorkbench *workbench;
  GtkRequisition req;
  GPtrArray *ar;
  GError *error = NULL;
  gsize i;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  ar = g_task_get_task_data (task);

  g_assert (GB_IS_APPLICATION (self));
  g_assert (ar);

  context = ide_context_new_finish (result, &error);

  if (!context)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      goto cleanup;
    }

  {
    IdeVcs *vcs;
    GFile *workdir;
    g_autofree gchar *path = NULL;

    vcs = ide_context_get_vcs (context);
    workdir = ide_vcs_get_working_directory (vcs);
    path = g_file_get_path (workdir);

    g_debug ("Project working directory: %s", path);
  }

  bufmgr = ide_context_get_buffer_manager (context);
  g_signal_connect (bufmgr, "create-buffer", G_CALLBACK (on_create_buffer), NULL);

  get_default_size (&req);

  workbench = g_object_new (GB_TYPE_WORKBENCH,
                            "application", self,
                            "context", context,
                            "default-width", req.width,
                            "default-height", req.height,
                            "title", _("Builder"),
                            NULL);

  if (ar->len == 0)
    gb_workbench_add_temporary_buffer (workbench);

  for (i = 0; i < ar->len; i++)
    {
      GFile *file;

      file = g_ptr_array_index (ar, i);
      g_assert (G_IS_FILE (file));

      gb_workbench_open (workbench, file);
    }

  gtk_window_maximize (GTK_WINDOW (workbench));
  gtk_window_present (GTK_WINDOW (workbench));

cleanup:
  g_task_return_boolean (task, FALSE);
  g_application_release (G_APPLICATION (self));
}

static void
gb_application_open (GApplication   *application,
                     GFile         **files,
                     gint            n_files,
                     const gchar    *hint)
{
  GbApplication *self = (GbApplication *)application;
  GbWorkbench *workbench;
  g_autoptr(GPtrArray) ar = NULL;
  guint i;

  IDE_ENTRY;

  g_assert (GB_IS_APPLICATION (self));

  /*
   * Try to open the files using an existing workbench.
   */
  for (i = 0; i < n_files; i++)
    {
      GFile *file = files [i];

      g_assert (G_IS_FILE (file));

      workbench = gb_application_find_workbench_for_file (self, file);

      if (workbench != NULL)
        {
          gb_workbench_open (workbench, file);
          gtk_window_present (GTK_WINDOW (workbench));
          continue;
        }

      if (!ar)
        ar = g_ptr_array_new_with_free_func (g_object_unref);
      g_ptr_array_add (ar, g_object_ref (file));
    }

  /*
   * No workbench found for these files, let's create one!
   */
  if (ar && ar->len)
    {
      g_autoptr(GFile) directory = NULL;
      g_autoptr(GTask) task = NULL;
      GFile *file;

      task = g_task_new (self, NULL, NULL, NULL);
      g_task_set_task_data (task, g_ptr_array_ref (ar), (GDestroyNotify)g_ptr_array_unref);

      file = g_ptr_array_index (ar, 0);

      if (g_file_query_file_type (file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
        directory = g_object_ref (file);
      else
        directory = g_file_get_parent (file);

      ide_context_new_async (directory,
                             NULL,
                             gb_application__context_new_cb,
                             g_object_ref (task));
      g_application_hold (G_APPLICATION (self));
    }

  IDE_EXIT;
}

static void
gb_application_activate (GApplication *application)
{
  GbApplication *self = (GbApplication *)application;
  const gchar *home_dir;
  g_autofree gchar *current_dir = NULL;
  g_autofree gchar *target_dir = NULL;
  g_autoptr(GFile) directory = NULL;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (GB_IS_APPLICATION (self));

  /*
   * FIXME:
   *
   * This is a stop gap until we get the project selection window in place.
   * That will allow us to select previous projects and such.
   */

  home_dir = g_get_home_dir ();
  current_dir = g_get_current_dir ();

  if (g_str_equal (home_dir, current_dir))
    target_dir = g_build_filename (home_dir, "Projects", NULL);
  else
    target_dir = g_strdup (current_dir);

  if (!g_file_test (target_dir, G_FILE_TEST_EXISTS))
    {
      g_free (target_dir);
      target_dir = g_strdup (home_dir);
    }

  directory = g_file_new_for_path (target_dir);

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_task_data (task, g_ptr_array_new (), (GDestroyNotify)g_ptr_array_unref);

  ide_context_new_async (directory,
                         NULL,
                         gb_application__context_new_cb,
                         g_object_ref (task));
  g_application_hold (application);
}

static void
gb_application_startup (GApplication *app)
{
  GbApplication *self = (GbApplication *)app;

  IDE_ENTRY;

  g_assert (GB_IS_APPLICATION (self));

  g_resources_register (gb_get_resource ());
  g_application_set_resource_base_path (app, "/org/gnome/builder");

  G_APPLICATION_CLASS (gb_application_parent_class)->startup (app);

  gb_application_make_skeleton_dirs (self);
  gb_application_actions_init (self);
  gb_application_register_theme_overrides (self);
  gb_application_setup_search_paths ();
  gb_application_load_keybindings (self);

  IDE_EXIT;
}

static gboolean
gb_application_increase_verbosity (void)
{
  ide_log_increase_verbosity ();
  return TRUE;
}

static gboolean
gb_application_local_command_line (GApplication   *app,
                                   gchar        ***argv,
                                   int            *exit_status)
{
  g_autoptr(GOptionContext) context = NULL;
  GOptionEntry entries[] = {
    { "verbose",
      'v',
      G_OPTION_FLAG_NO_ARG|G_OPTION_FLAG_IN_MAIN,
      G_OPTION_ARG_CALLBACK,
      gb_application_increase_verbosity,
      N_("Increase verbosity. May be specified multiple times.") },
    { NULL }
  };

  /* we dont really care about the result, just increasing verbosity */
  context = g_option_context_new ("");
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_parse_strv (context, argv, NULL);

  return G_APPLICATION_CLASS (gb_application_parent_class)->local_command_line (app,
                                                                                argv,
                                                                                exit_status);
}

static void
gb_application_finalize (GObject *object)
{
  GbApplication *self = (GbApplication *)object;

  IDE_ENTRY;

  g_clear_object (&self->keybindings);
  g_clear_object (&self->editor_settings);

  G_OBJECT_CLASS (gb_application_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
gb_application_class_init (GbApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  IDE_ENTRY;

  object_class->finalize = gb_application_finalize;

  app_class->activate = gb_application_activate;
  app_class->startup = gb_application_startup;
  app_class->open = gb_application_open;
  app_class->local_command_line = gb_application_local_command_line;

  IDE_EXIT;
}

static void
gb_application_init (GbApplication *application)
{
  IDE_ENTRY;
  IDE_EXIT;
}
