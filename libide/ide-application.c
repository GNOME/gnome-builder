/* ide-application.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>
#include <girepository.h>
#include <gtksourceview/gtksource.h>
#include <libgit2-glib/ggit.h>
#include <locale.h>
#include <stdlib.h>
#include <sys/prctl.h>

#include "ide-application.h"
#include "ide-application-actions.h"
#include "ide-application-private.h"
#include "ide-application-tool.h"
#include "ide-css-provider.h"
#include "ide-debug.h"
#include "ide-global.h"
#include "ide-icons-resources.h"
#include "ide-internal.h"
#include "ide-macros.h"
#include "ide-resources.h"
#include "ide-workbench.h"
#include "ide-worker.h"

#include "modeline-parser.h"

G_DEFINE_TYPE (IdeApplication, ide_application, GTK_TYPE_APPLICATION)

static void
ide_application_make_skeleton_dirs (IdeApplication *self)
{
  gchar *path;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_APPLICATION (self));

  path = g_build_filename (g_get_user_data_dir (), "gnome-builder", NULL);
  g_mkdir_with_parents (path, 0750);
  g_free (path);

  path = g_build_filename (g_get_user_config_dir (), "gnome-builder", NULL);
  g_mkdir_with_parents (path, 0750);
  g_free (path);

  path = g_build_filename (g_get_user_config_dir (), "gnome-builder", "snippets", NULL);
  g_mkdir_with_parents (path, 0750);
  g_free (path);

  IDE_EXIT;
}

static void
ide_application_register_theme_overrides (IdeApplication *self)
{
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GtkCssProvider) provider = NULL;
  GtkSettings *gtk_settings;
  GdkScreen *screen;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                                    "/org/gnome/builder/icons/");

  provider = ide_css_provider_new ();
  screen = gdk_screen_get_default ();
  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_settings = gtk_settings_get_for_screen (screen);
  settings = g_settings_new ("org.gnome.builder");
  g_settings_bind (settings, "night-mode",
                   gtk_settings, "gtk-application-prefer-dark-theme",
                   G_SETTINGS_BIND_DEFAULT);

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
  self->keybindings = ide_keybindings_new (GTK_APPLICATION (self), name);
  g_settings_bind (settings, "keybindings", self->keybindings, "mode", G_SETTINGS_BIND_GET);

  IDE_EXIT;
}

static void
ide_application_register_search_paths (IdeApplication *self)
{
  g_assert (IDE_IS_APPLICATION (self));

  gtk_source_style_scheme_manager_append_search_path (gtk_source_style_scheme_manager_get_default (),
                                                      PACKAGE_DATADIR"/gtksourceview-3.0/styles/");
  g_irepository_prepend_search_path (PACKAGE_LIBDIR"/gnome-builder/girepository-1.0");
}

static void
ide_application_register_ggit (IdeApplication *self)
{
  GgitFeatureFlags ggit_flags;

  g_assert (IDE_IS_APPLICATION (self));

  ggit_init ();

  ggit_flags = ggit_get_features ();

  if ((ggit_flags & GGIT_FEATURE_THREADS) == 0)
    {
      g_error (_("Builder requires libgit2-glib with threading support."));
      exit (EXIT_FAILURE);
    }

  if ((ggit_flags & GGIT_FEATURE_SSH) == 0)
    {
      g_error (_("Builder requires libgit2-glib with SSH support."));
      exit (EXIT_FAILURE);
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

#ifdef __linux
  /* Ensure we are killed with our parent */
  prctl (PR_SET_PDEATHSIG, 15);
#endif

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
}

static void
ide_application_startup (GApplication *application)
{
  IdeApplication *self = (IdeApplication *)application;
  gboolean small_thread_pool;

  g_assert (IDE_IS_APPLICATION (self));

  g_resources_register (ide_get_resource ());
  g_resources_register (ide_icons_get_resource ());

  g_application_set_resource_base_path (application, "/org/gnome/builder");
  ide_application_register_search_paths (self);

  small_thread_pool = (self->mode != IDE_APPLICATION_MODE_PRIMARY);
  _ide_thread_pool_init (small_thread_pool);

  if (self->mode == IDE_APPLICATION_MODE_PRIMARY)
    {
      ide_application_make_skeleton_dirs (self);
      ide_application_register_theme_overrides (self);
      ide_application_register_keybindings (self);
      ide_application_register_ggit (self);
      ide_application_actions_init (self);

      modeline_parser_init ();
    }

  _ide_battery_monitor_init ();

  G_APPLICATION_CLASS (ide_application_parent_class)->startup (application);

  ide_application_load_addins (self);
}

static void
ide_application_open (GApplication  *application,
                      GFile        **files,
                      gint           n_files,
                      const gchar   *hint)
{
  g_assert (IDE_IS_APPLICATION (application));

}

static void
ide_application_finalize (GObject *object)
{
  IdeApplication *self = (IdeApplication *)object;

  g_clear_pointer (&self->dbus_address, g_free);
  g_clear_pointer (&self->tool_arguments, g_strfreev);
  g_clear_pointer (&self->started_at, g_date_time_unref);
  g_clear_object (&self->worker_manager);
  g_clear_object (&self->keybindings);
  g_clear_object (&self->recent_projects);

  G_OBJECT_CLASS (ide_application_parent_class)->finalize (object);
}

static void
ide_application_class_init (IdeApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *g_app_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = ide_application_finalize;

  g_app_class->activate = ide_application_activate;
  g_app_class->local_command_line = ide_application_local_command_line;
  g_app_class->open = ide_application_open;
  g_app_class->startup = ide_application_startup;
}

static void
ide_application_init (IdeApplication *self)
{
  ide_set_program_name (PACKAGE_NAME);

  self->started_at = g_date_time_new_now_utc ();
  self->mode = IDE_APPLICATION_MODE_PRIMARY;

  setlocale (LC_ALL, "");

  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_set_application_name (_("Builder"));
}

IdeApplication *
ide_application_new (void)
{
  return g_object_new (IDE_TYPE_APPLICATION,
                       "application-id", "org.gnome.Builder",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);

}

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
  GError *error = NULL;
  GDBusProxy *proxy;

  g_assert (IDE_IS_WORKER_MANAGER (worker_manager));

  proxy = ide_worker_manager_get_worker_finish (worker_manager, result, &error);

  if (proxy == NULL)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, proxy, g_object_unref);
}

/**
 * ide_application_get_worker_async:
 * @self: A #IdeApplication
 * @plugin_name: The name of the plugin.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback or %NULL.
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
  GTask *task = NULL;

  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (plugin_name != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->mode != IDE_APPLICATION_MODE_PRIMARY)
    return NULL;

  if (self->worker_manager == NULL)
    self->worker_manager = ide_worker_manager_new ();

  task = g_task_new (self, cancellable, callback, user_data);

  ide_worker_manager_get_worker_async (self->worker_manager,
                                       plugin_name,
                                       cancellable,
                                       ide_application_get_worker_cb,
                                       task);
}

/**
 * ide_application_get_worker_finish:
 * @self: A #IdeApplication.
 * @result: A #GAsyncResult
 * @error: a location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to get a proxy to a worker process.
 *
 * Returns: (transfer full): A #GDBusProxy or %NULL.
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
 * represents recent and discover projects on the system. The first time
 * the #IdeRecentProjects is loaded, discovery of projects will occur. There
 * is no need to call ide_recent_projects_discover_async().
 *
 * If you would like to display a spinner while discovery is in process, simply
 * connect to the #IdeRecentProjects:busy: property notification.
 *
 * Returns: (transfer none): An #IdeRecentProjects.
 */
IdeRecentProjects *
ide_application_get_recent_projects (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  if (self->mode != IDE_APPLICATION_MODE_PRIMARY)
    return NULL;

  if (self->recent_projects == NULL)
    {
      self->recent_projects = ide_recent_projects_new ();
      ide_recent_projects_discover_async (self->recent_projects, NULL, NULL, NULL);
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

          if (ide_str_equal0 ("greeter", name))
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
 * @self: A #IdeApplication.
 *
 * Gets the startup time of the application.
 *
 * Returns: (transfer none): A #GDateTime.
 */
GDateTime *
ide_application_get_started_at (IdeApplication *self)
{
  return self->started_at;
}
