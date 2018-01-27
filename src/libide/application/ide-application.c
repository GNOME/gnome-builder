/* ide-application.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-application"

#include "config.h"

#include <glib/gi18n.h>
#include <girepository.h>
#include <gtksourceview/gtksource.h>
#include <ide-icons-resources.h>
#include <locale.h>
#include <stdlib.h>

#include "ide-debug.h"
#include "ide-global.h"
#include "ide-resources.h"

#include "application/ide-application.h"
#include "application/ide-application-actions.h"
#include "application/ide-application-private.h"
#include "application/ide-application-tests.h"
#include "application/ide-application-tool.h"
#include "modelines/modeline-parser.h"
#include "threading/ide-thread-pool.h"
#include "util/ide-battery-monitor.h"
#include "util/ide-flatpak.h"
#include "util/ide-posix.h"
#include "workbench/ide-workbench.h"
#include "workers/ide-worker.h"

/**
 * SECTION:ide-application
 * @title: IdeApplication
 * @short_description: Application singleton and extensions
 *
 * The #IdeApplication class is a singleton whose lifetime is synchronized to
 * the lifetime of the process. This manages necessary tweaks required by the
 * application (such as menus, theming, and keybindings) as well as application
 * plugins.
 *
 * ## Extensions
 *
 * If you need to extend Builder in a way that requires services that are tied
 * to the lifetime of the process (rather than the lifetime of the project),
 * then #IdeApplicationAddin provides the extension point you need.
 *
 * Since: 3.18
 */

G_DEFINE_TYPE (IdeApplication, ide_application, DZL_TYPE_APPLICATION)

static GThread *main_thread;
static const gchar *legacy_dirs[] = {
  "buffers",
  "builds",
  "code-index",
  "flatpak",
  "install",
  "tags",
  NULL,
};

void
_ide_application_set_mode (IdeApplication     *self,
                           IdeApplicationMode  mode)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));

  self->mode = mode;
}

static void
ide_application_make_skeleton_dirs (IdeApplication *self)
{
  g_autoptr(GFile) projects_dir = NULL;
  gchar *path;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_APPLICATION (self));

  path = g_build_filename (g_get_user_data_dir (), "gnome-builder", NULL);
  g_mkdir_with_parents (path, 0750);
  g_clear_pointer (&path, g_free);

  path = g_build_filename (g_get_user_config_dir (), "gnome-builder", NULL);
  g_mkdir_with_parents (path, 0750);
  g_clear_pointer (&path, g_free);

  path = g_build_filename (g_get_user_config_dir (), "gnome-builder", "snippets", NULL);
  g_mkdir_with_parents (path, 0750);
  g_clear_pointer (&path, g_free);

  projects_dir = ide_application_get_projects_directory (self);
  g_assert (G_IS_FILE (projects_dir));

  if (!g_file_query_exists (projects_dir, NULL))
    g_file_make_directory_with_parents (projects_dir, NULL, NULL);

  IDE_EXIT;
}

static void
ide_application_register_keybindings (IdeApplication *self)
{
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *name = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  settings = g_settings_new ("org.gnome.builder.editor");
  name = g_settings_get_string (settings, "keybindings");
  self->keybindings = ide_keybindings_new (name);
  g_settings_bind (settings, "keybindings", self->keybindings, "mode", G_SETTINGS_BIND_GET);

  IDE_EXIT;
}

static void
ide_application_register_plugin_accessories (IdeApplication *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  ide_application_init_plugin_accessories (self);

  IDE_EXIT;
}

static void
ide_application_register_search_paths (IdeApplication *self)
{
  GtkSourceStyleSchemeManager *manager;
  GtkSourceLanguageManager *languages;
  g_autofree gchar *gedit_path = NULL;
  g_autofree gchar *lang_path = NULL;
  g_autofree gchar *style_path = NULL;
  const gchar * const *path;

  g_assert (IDE_IS_APPLICATION (self));

  manager = gtk_source_style_scheme_manager_get_default ();

  /* We might need to set this up in case we're in flatpak */
  style_path = g_build_filename (g_get_home_dir (),
                                 ".local",
                                 "share",
                                 "gtksourceview-3.0",
                                 "styles",
                                 NULL);
  gtk_source_style_scheme_manager_append_search_path (manager, style_path);

  gtk_source_style_scheme_manager_append_search_path (manager,
                                                      PACKAGE_DATADIR"/gtksourceview-3.0/styles/");

  /* We can use styles from gedit too */
  gedit_path = g_build_filename (g_get_user_data_dir (), "gedit", "styles", NULL);
  gtk_source_style_scheme_manager_append_search_path (manager, gedit_path);

  if (g_getenv ("GB_IN_TREE_STYLE_SCHEMES") != NULL)
    gtk_source_style_scheme_manager_prepend_search_path (manager, SRCDIR"/data/style-schemes");

  /* Make sure we load user-installed style schemes if using non-standard dirs */
  languages = gtk_source_language_manager_get_default ();
  path = gtk_source_language_manager_get_search_path (languages);
  lang_path = g_build_filename (g_get_home_dir (),
                                ".local",
                                "share",
                                "gtksourceview-3.0",
                                "language-specs",
                                NULL);
  if (!g_strv_contains (path, lang_path))
    {
      g_autoptr(GPtrArray) ar = g_ptr_array_new ();

      g_ptr_array_add (ar, lang_path);
      for (guint i = 0; path[i]; i++)
        g_ptr_array_add (ar, (gchar *)path[i]);
      g_ptr_array_add (ar, NULL);

      gtk_source_language_manager_set_search_path (languages, (gchar **)(gpointer)ar->pdata);
    }
}

static void
ide_application_activate_primary (IdeApplication *self)
{
  GtkWindow *window;
  GList *windows;

  g_assert (IDE_IS_APPLICATION (self));

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (; windows; windows = windows->next)
    {
      window = windows->data;

      if (IDE_IS_WORKBENCH (window))
        {
          gtk_window_present (window);
          return;
        }
    }

  window = g_object_new (IDE_TYPE_WORKBENCH,
                         "application", self,
                         NULL);
  gtk_window_present (window);
}

static void
ide_application_activate_worker (IdeApplication *self)
{
  g_autoptr(GDBusConnection) connection = NULL;
  PeasExtension *extension;
  PeasEngine *engine;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (self->worker != NULL);
  g_assert (self->dbus_address != NULL);

  IDE_TRACE_MSG ("Connecting to %s", self->dbus_address);

  connection = g_dbus_connection_new_for_address_sync (self->dbus_address,
                                                       (G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                        G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING),
                                                       NULL, NULL, &error);

  if (error != NULL)
    {
      g_error ("DBus failure: %s", error->message);
      g_clear_error (&error);
      IDE_EXIT;
    }

  engine = peas_engine_get_default ();
  extension = peas_engine_create_extension (engine, self->worker, IDE_TYPE_WORKER, NULL);

  if (extension == NULL)
    {
      g_error ("Failed to create \"%s\" worker",
               peas_plugin_info_get_module_name (self->worker));
      IDE_EXIT;
    }

  ide_worker_register_service (IDE_WORKER (extension), connection);
  g_application_hold (G_APPLICATION (self));
  g_dbus_connection_start_message_processing (connection);

  IDE_EXIT;
}

static void
ide_application_activate_tool_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeApplicationTool *tool = (IdeApplicationTool *)object;
  g_autoptr(IdeApplication) self = user_data;
  GError *error = NULL;
  gint exit_code;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (IDE_IS_APPLICATION_TOOL (tool));

  exit_code = ide_application_tool_run_finish (tool, result, &error);

  if (error != NULL)
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
    }

  /* GApplication does not provide a way to pass exit code. */
  if (exit_code != 0)
    exit (exit_code);

  g_application_release (G_APPLICATION (self));
}

static void
ide_application_activate_tool (IdeApplication *self)
{
  PeasEngine *engine;
  PeasExtension *tool;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (self->tool != NULL);
  g_assert (self->tool_arguments != NULL);

  engine = peas_engine_get_default ();
  tool = peas_engine_create_extension (engine,
                                       self->tool,
                                       IDE_TYPE_APPLICATION_TOOL,
                                       NULL);
  if (tool == NULL)
    return;

  g_application_hold (G_APPLICATION (self));

  ide_application_tool_run_async (IDE_APPLICATION_TOOL (tool),
                                  (const gchar * const *)self->tool_arguments,
                                  NULL,
                                  ide_application_activate_tool_cb,
                                  g_object_ref (self));

  g_object_unref (tool);
}

static void
ide_application_activate_tests (IdeApplication *self)
{
  g_assert (IDE_IS_APPLICATION (self));

  ide_application_run_tests (self);
}

static void
ide_application_activate (GApplication *application)
{
  IdeApplication *self = (IdeApplication *)application;

  g_assert (IDE_IS_APPLICATION (self));

  if (self->mode == IDE_APPLICATION_MODE_PRIMARY)
    ide_application_activate_primary (self);
  else if (self->mode == IDE_APPLICATION_MODE_WORKER)
    ide_application_activate_worker (self);
  else if (self->mode == IDE_APPLICATION_MODE_TOOL)
    ide_application_activate_tool (self);
  else if (self->mode == IDE_APPLICATION_MODE_TESTS)
    ide_application_activate_tests (self);
}

static void
ide_application_language_defaults_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GError *error = NULL;
  G_GNUC_UNUSED gboolean ret;

  ret = ide_language_defaults_init_finish (result, &error);

  if (error != NULL)
    {
      g_warning ("%s\n", error->message);
      g_clear_error (&error);
    }
}

static void
ide_application_register_settings (IdeApplication *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  if (g_getenv ("GTK_THEME") == NULL)
    {
      GtkSettings *gtk_settings = gtk_settings_get_default ();

      g_settings_bind (self->settings, "night-mode",
                       gtk_settings, "gtk-application-prefer-dark-theme",
                       G_SETTINGS_BIND_DEFAULT);
    }

  IDE_EXIT;
}

static GFile *
build_legacy_cache_directory (const gchar *name)
{
  return g_file_new_build_filename (g_get_user_cache_dir (),
                                    ide_get_program_name (),
                                    name,
                                    NULL);
}

static void
ide_application_reap_legacy_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  DzlDirectoryReaper *reaper = (DzlDirectoryReaper *)object;
  g_autoptr(IdeApplication) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (DZL_IS_DIRECTORY_REAPER (reaper));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_APPLICATION (self));

  if (!dzl_directory_reaper_execute_finish (reaper, result, &error))
    {
      g_warning ("Failure reaping legacy data: %s", error->message);
      IDE_EXIT;
    }

  for (guint i = 0; legacy_dirs[i] != NULL; i++)
    {
      g_autoptr(GFile) directory = NULL;
      const gchar *name = legacy_dirs[i];

      directory = build_legacy_cache_directory (name);
      g_file_delete_async (directory, G_PRIORITY_LOW, NULL, NULL, NULL);
    }

  IDE_EXIT;
}

/*
 * _ide_application_reap_legacy:
 *
 * This is meant to remove a bunch of legacy directories we no longer use.
 * Since it can take a while, we do it in the background at startup if we
 * discover they are there.
 */
static void
_ide_application_reap_legacy (IdeApplication *self)
{
  g_autoptr(DzlDirectoryReaper) reaper = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  reaper = dzl_directory_reaper_new ();

  /* Cleanup a number of old directories no longer used */
  for (guint i = 0; legacy_dirs[i] != NULL; i++)
    {
      g_autoptr(GFile) directory = NULL;
      const gchar *name = legacy_dirs[i];

      directory = build_legacy_cache_directory (name);
      dzl_directory_reaper_add_directory (reaper, directory, 0);
    }

  /* Do this asynchronously so we don't block the program at shutdown. */
  dzl_directory_reaper_execute_async (reaper,
                                      NULL,
                                      ide_application_reap_legacy_cb,
                                      g_object_ref (self));

  IDE_EXIT;
}

static void
ide_application_startup (GApplication *application)
{
  IdeApplication *self = (IdeApplication *)application;
  gboolean small_thread_pool;

  g_assert (IDE_IS_APPLICATION (self));

  self->settings = g_settings_new ("org.gnome.builder");

  g_resources_register (ide_get_resource ());
  g_resources_register (ide_icons_get_resource ());

  g_application_set_resource_base_path (application, "/org/gnome/builder/");
  ide_application_register_search_paths (self);

  small_thread_pool = (self->mode != IDE_APPLICATION_MODE_PRIMARY);
  _ide_thread_pool_init (small_thread_pool);

  if ((self->mode == IDE_APPLICATION_MODE_PRIMARY) || (self->mode == IDE_APPLICATION_MODE_TESTS))
    {
      self->transfer_manager = g_object_new (IDE_TYPE_TRANSFER_MANAGER, NULL);

      ide_application_make_skeleton_dirs (self);
      ide_language_defaults_init_async (NULL, ide_application_language_defaults_cb, NULL);
      ide_application_register_settings (self);
      ide_application_register_keybindings (self);
      ide_application_actions_init (self);
      _ide_application_init_shortcuts (self);
      _ide_application_init_color (self);
      _ide_application_reap_legacy (self);

      modeline_parser_init ();
    }

  _ide_battery_monitor_init ();

  G_APPLICATION_CLASS (ide_application_parent_class)->startup (application);

  if (self->mode == IDE_APPLICATION_MODE_PRIMARY)
    ide_application_register_plugin_accessories (self);

  ide_application_load_addins (self);
}

static void
ide_application_open (GApplication  *application,
                      GFile        **files,
                      gint           n_files,
                      const gchar   *hint)
{
  g_assert (IDE_IS_APPLICATION (application));

  ide_application_open_async (IDE_APPLICATION (application),
                              files,
                              n_files,
                              hint,
                              NULL,
                              NULL,
                              NULL);
}

static gboolean
ide_application_force_exit_timeout (gpointer data)
{
  g_warning ("Plugin caught spinning, forcing exit immediately");
  exit (EXIT_FAILURE);
  return G_SOURCE_REMOVE;
}

static void
ide_application_shutdown (GApplication *application)
{
  IdeApplication *self = (IdeApplication *)application;

  if (self->worker_manager != NULL)
    ide_worker_manager_shutdown (self->worker_manager);

  g_clear_object (&self->transfer_manager);

  if (G_APPLICATION_CLASS (ide_application_parent_class)->shutdown)
    G_APPLICATION_CLASS (ide_application_parent_class)->shutdown (application);

  /* Run all reapers serially on shutdown */

  for (guint i = 0; i < self->reapers->len; i++)
    {
      DzlDirectoryReaper *reaper = g_ptr_array_index (self->reapers, i);
      g_autoptr(GError) error = NULL;

      g_assert (DZL_IS_DIRECTORY_REAPER (reaper));

      if (!dzl_directory_reaper_execute (reaper, NULL, &error))
        g_warning ("Failed to reap files: %s", error->message);
    }

  _ide_battery_monitor_shutdown ();

  /* In case a plugin causes us to spin at shutdown, make sure that we
   * force exit to avoid spinning in the background.
   */
  gdk_threads_add_timeout (3000, /* 3 seconds */
                           ide_application_force_exit_timeout,
                           NULL);
}

static void
ide_application_window_added (GtkApplication *application,
                              GtkWindow      *window)
{
  IdeApplication *self = (IdeApplication *)application;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (GTK_IS_WINDOW (window));

  GTK_APPLICATION_CLASS (ide_application_parent_class)->window_added (application, window);

  ide_application_actions_update (self);
}

static void
ide_application_window_removed (GtkApplication *application,
                                GtkWindow      *window)
{
  IdeApplication *self = (IdeApplication *)application;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (GTK_IS_WINDOW (window));

  GTK_APPLICATION_CLASS (ide_application_parent_class)->window_removed (application, window);

  ide_application_actions_update (self);
}

static void
ide_application_finalize (GObject *object)
{
  IdeApplication *self = (IdeApplication *)object;

  g_clear_pointer (&self->test_funcs, g_list_free);
  g_clear_pointer (&self->dbus_address, g_free);
  g_clear_pointer (&self->tool_arguments, g_strfreev);
  g_clear_pointer (&self->started_at, g_date_time_unref);
  g_clear_pointer (&self->plugin_css, g_hash_table_unref);
  g_clear_pointer (&self->plugin_settings, g_hash_table_unref);
  g_clear_pointer (&self->reapers, g_ptr_array_unref);
  g_clear_pointer (&self->plugin_gresources, g_hash_table_unref);
  g_clear_object (&self->worker_manager);
  g_clear_object (&self->keybindings);
  g_clear_object (&self->recent_projects);
  g_clear_object (&self->settings);
  g_clear_object (&self->color_proxy);

  G_OBJECT_CLASS (ide_application_parent_class)->finalize (object);
}

static void
ide_application_class_init (IdeApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *g_app_class = G_APPLICATION_CLASS (klass);
  GtkApplicationClass *gtk_app_class = GTK_APPLICATION_CLASS (klass);

  object_class->finalize = ide_application_finalize;

  g_app_class->activate = ide_application_activate;
  g_app_class->local_command_line = ide_application_local_command_line;
  g_app_class->open = ide_application_open;
  g_app_class->startup = ide_application_startup;
  g_app_class->shutdown = ide_application_shutdown;

  gtk_app_class->window_added = ide_application_window_added;
  gtk_app_class->window_removed = ide_application_window_removed;

  main_thread = g_thread_self ();
}

static void
ide_application_init (IdeApplication *self)
{
  ide_set_program_name (PACKAGE_NAME);

  self->reapers = g_ptr_array_new_with_free_func (g_object_unref);

  self->started_at = g_date_time_new_now_utc ();
  self->mode = IDE_APPLICATION_MODE_PRIMARY;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_set_application_name (_("Builder"));
  gtk_window_set_default_icon_name ("org.gnome.Builder");
}

/**
 * ide_application_new:
 *
 * Creates a new #IdeApplication. This should only be used by the application
 * entry point.
 *
 * Since: 3.22
 */
IdeApplication *
ide_application_new (void)
{
  return g_object_new (IDE_TYPE_APPLICATION,
                       "application-id", "org.gnome.Builder",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

/**
 * ide_application_get_mode:
 * @self: An #IdeApplication
 *
 * Gets the mode of the application which describes if we are the UI
 * process, worker process, or internal test runner.
 *
 * Returns: the #IdeApplicationMode
 *
 * Since: 3.20
 */
IdeApplicationMode
ide_application_get_mode (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), 0);

  return self->mode;
}

static void
ide_application_get_worker_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeWorkerManager *worker_manager = (IdeWorkerManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GDBusProxy *proxy;

  g_assert (IDE_IS_WORKER_MANAGER (worker_manager));

  proxy = ide_worker_manager_get_worker_finish (worker_manager, result, &error);

  if (proxy == NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, proxy, g_object_unref);
}

/**
 * ide_application_get_worker_async:
 * @self: an #IdeApplication
 * @plugin_name: The name of the plugin.
 * @cancellable: (allow-none): a #GCancellable or %NULL.
 * @callback: a #GAsyncReadyCallback or %NULL.
 * @user_data: user data for @callback.
 *
 * Asynchronously requests a #GDBusProxy to a service provided in a worker
 * process. The worker should be an #IdeWorker implemented by the plugin named
 * @plugin_name. The #IdeWorker is responsible for created both the service
 * registered on the bus and the proxy to it.
 *
 * The #IdeApplication is responsible for spawning a subprocess for the worker.
 *
 * @callback should call ide_application_get_worker_finish() with the result
 * provided to retrieve the result.
 */
void
ide_application_get_worker_async (IdeApplication      *self,
                                  const gchar         *plugin_name,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (plugin_name != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->mode != IDE_APPLICATION_MODE_PRIMARY)
    return;

  if (self->worker_manager == NULL)
    self->worker_manager = ide_worker_manager_new ();

  task = g_task_new (self, cancellable, callback, user_data);

  ide_worker_manager_get_worker_async (self->worker_manager,
                                       plugin_name,
                                       cancellable,
                                       ide_application_get_worker_cb,
                                       g_object_ref (task));
}

/**
 * ide_application_get_worker_finish:
 * @self: an #IdeApplication.
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to get a proxy to a worker process.
 *
 * Returns: (transfer full): a #GDBusProxy or %NULL.
 */
GDBusProxy *
ide_application_get_worker_finish (IdeApplication  *self,
                                   GAsyncResult    *result,
                                   GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

/**
 * ide_application_get_recent_projects:
 * @self: An #IdeApplication.
 *
 * This method will retreive an #IdeRecentProjects for the application that
 * represents recent and discover projects on the system.
 *
 * Returns: (transfer none): An #IdeRecentProjects.
 */
IdeRecentProjects *
ide_application_get_recent_projects (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  if (self->mode == IDE_APPLICATION_MODE_PRIMARY)
    {
      if (self->recent_projects == NULL)
        self->recent_projects = ide_recent_projects_new ();
    }

  return self->recent_projects;
}

void
ide_application_show_projects_window (IdeApplication *self)
{
  GtkWindow *window;
  GList *windows;

  g_assert (IDE_IS_APPLICATION (self));

  if (self->mode != IDE_APPLICATION_MODE_PRIMARY)
    return;

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (; windows; windows = windows->next)
    {
      window = windows->data;

      if (IDE_IS_WORKBENCH (window))
        {
          const gchar *name;

          name = ide_workbench_get_visible_perspective_name (IDE_WORKBENCH (window));

          if (dzl_str_equal0 ("greeter", name))
            {
              gtk_window_present (windows->data);
              return;
            }
        }
    }

  window = g_object_new (IDE_TYPE_WORKBENCH,
                         "application", self,
                         NULL);
  gtk_window_present (window);
}

const gchar *
ide_application_get_keybindings_mode (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  if (self->mode == IDE_APPLICATION_MODE_PRIMARY)
    return ide_keybindings_get_mode (self->keybindings);

  return NULL;
}

/**
 * ide_application_get_started_at:
 * @self: an #IdeApplication.
 *
 * Gets the startup time of the application.
 *
 * Returns: (transfer none): a #GDateTime.
 */
GDateTime *
ide_application_get_started_at (IdeApplication *self)
{
  return self->started_at;
}

/**
 * ide_application_open_project:
 * @self: a #IdeApplication
 * @file: a #GFile
 *
 * Attempts to load the project found at @file.
 *
 * Returns: %TRUE if the project is already open, otherwise %FALSE.
 *
 * Since: 3.22
 */
gboolean
ide_application_open_project (IdeApplication *self,
                              GFile          *file)
{
  GList *list;
  IdeContext *context;
  GtkWindow *window;
  GFile *projectfile;
  IdeWorkbench *workbench = NULL;

  g_return_val_if_fail (IDE_IS_APPLICATION (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  /*
   * TODO: I don't like how this works. We should move this to
   *       be async anyway and possibly share it with the open
   *       file async code. Additionally, it has a race condition
   *       for situations where the context was not loaded
   *       immediately (and that will always happen).
   */

  if (!g_file_query_exists (file, NULL))
    return FALSE;

  list = gtk_application_get_windows (GTK_APPLICATION (self));

  for (; list != NULL; list = list->next)
    {
      window = list->data;
      context = ide_workbench_get_context (IDE_WORKBENCH (window));

      if (context != NULL)
        {
          projectfile = g_file_get_parent (ide_context_get_project_file (context));
          if (g_file_equal (file, projectfile))
            workbench =  IDE_WORKBENCH (window);
        }
    }

  if (workbench == NULL)
    {
      workbench = g_object_new (IDE_TYPE_WORKBENCH,
                                "application", self,
                                "disable-greeter", TRUE,
                                NULL);
      ide_workbench_open_project_async (workbench, file, NULL, NULL, NULL);
    }

  gtk_window_present (GTK_WINDOW (workbench));

  if (ide_workbench_get_context (workbench) != NULL)
    return TRUE;
  else
    return FALSE;
}

/**
 * ide_application_get_main_thread:
 *
 * This function returns the thread-id of the main thread for the application.
 * This is only really useful to determine if you are in the main UI thread.
 * This is used by IDE_IS_MAIN_THREAD for assertion checks.
 *
 * Returns: (transfer none): a #GThread
 */
GThread *
ide_application_get_main_thread (void)
{
  return main_thread;
}

/**
 * ide_application_add_reaper:
 * @self: a #IdeApplication
 * @reaper: a #DzlDirectoryReaper
 *
 * Adds a directory reaper which will be executed as part of the cleanup
 * process when exiting Builder.
 *
 * Since: 3.24
 */
void
ide_application_add_reaper (IdeApplication     *self,
                            DzlDirectoryReaper *reaper)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (DZL_IS_DIRECTORY_REAPER (reaper));

  g_ptr_array_add (self->reapers, g_object_ref (reaper));
}

/**
 * ide_application_get_transfer_manager:
 * @self: a #IdeApplication
 *
 * Gets the transfer manager for the application.
 *
 * Returns: (transfer none): An #IdeTransferManager
 *
 * Since: 3.28
 */
IdeTransferManager *
ide_application_get_transfer_manager (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  return self->transfer_manager;
}

/**
 * ide_application_get_projects_directory:
 * @self: an #IdeApplication
 *
 * Gets the directory to store projects within.
 *
 * First, this checks GSettings for a directory. If that directory exists,
 * it is returned.
 *
 * If not, it then checks for the non-translated name "Projects" in the
 * users home directory. If it exists, that is returned.
 *
 * If that does not exist, and a GSetting path existed, but was non-existant
 * that is returned.
 *
 * If the GSetting was empty, the translated name "Projects" is returned.
 *
 * Returns: (not nullable) (transfer full): a #GFile
 *
 * Since: 3.28
 */
GFile *
ide_application_get_projects_directory (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);
  g_return_val_if_fail (G_IS_SETTINGS (self->settings), NULL);

  if (self->projects_directory == NULL)
    {
      g_autofree gchar *dir = g_settings_get_string (self->settings, "projects-directory");
      g_autofree gchar *expanded = ide_path_expand (dir);
      g_autofree gchar *projects = NULL;
      g_autofree gchar *translated = NULL;
      g_autoptr(GFile) ret = NULL;

      if (g_file_test (expanded, G_FILE_TEST_IS_DIR))
        {
          ret = g_file_new_for_path (expanded);
          goto completed;
        }

      projects = g_build_filename (g_get_home_dir (), "Projects", NULL);

      if (g_file_test (projects, G_FILE_TEST_IS_DIR))
        {
          ret = g_file_new_for_path (projects);
          goto completed;
        }

      if (!dzl_str_empty0 (dir) && !dzl_str_empty0 (expanded))
        {
          ret = g_file_new_for_path (expanded);
          goto completed;
        }

      translated = g_build_filename (g_get_home_dir (), _("Projects"), NULL);
      ret = g_file_new_for_path (translated);

    completed:
      g_assert (G_IS_FILE (ret));

      self->projects_directory = g_steal_pointer (&ret);
    }

  return g_object_ref (self->projects_directory);
}
