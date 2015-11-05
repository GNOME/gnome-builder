/* ide-application.c
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

#define G_LOG_DOMAIN "ide-application"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef __linux
# include <sys/prctl.h>
#endif

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libgit2-glib/ggit.h>

#include "ide-application.h"
#include "ide-application-actions.h"
#include "ide-application-addin.h"
#include "ide-application-private.h"
#include "ide-css-provider.h"
#include "ide-debug.h"
#include "ide-internal.h"
#include "ide-file.h"
#include "ide-log.h"
#include "ide-macros.h"
#include "ide-resources.h"
#include "ide-vcs.h"
#include "ide-workbench.h"
#include "ide-worker.h"

#include "modeline-parser.h"

G_DEFINE_TYPE (IdeApplication, ide_application, GTK_TYPE_APPLICATION)

static gboolean
ide_application_can_load_plugin (IdeApplication *self,
                                 PeasPluginInfo *plugin_info)
{
  const gchar *plugin_name;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);

  /* Currently we only allow in-tree plugins */
  if (!peas_plugin_info_is_builtin (plugin_info))
    return FALSE;

  plugin_name = peas_plugin_info_get_module_name (plugin_info);

  /* If --type was specified, only that plugin may be loaded */
  if ((self->type != NULL) && !ide_str_equal0 (plugin_name, self->type))
    return FALSE;

  return TRUE;
}

static void
ide_application_load_plugins (IdeApplication *self)
{
  PeasEngine *engine = peas_engine_get_default ();
  const GList *list;

  peas_engine_enable_loader (engine, "python3");

  if (g_getenv ("GB_IN_TREE_PLUGINS") != NULL)
    {
      GDir *dir;

      g_irepository_require_private (g_irepository_get_default (),
                                     BUILDDIR"/libide",
                                     "Ide", "1.0", 0, NULL);

      if ((dir = g_dir_open (BUILDDIR"/plugins", 0, NULL)))
        {
          const gchar *name;

          while ((name = g_dir_read_name (dir)))
            {
              gchar *path;

              path = g_build_filename (BUILDDIR, "plugins", name, NULL);
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
      if (ide_application_can_load_plugin (self, list->data))
        peas_engine_load_plugin (engine, list->data);
    }
}

static gboolean
ide_application_is_worker (IdeApplication *self)
{
  g_assert (IDE_IS_APPLICATION (self));

  return (self->type != NULL) && (self->dbus_address != NULL);
}

static void
ide_application_load_worker (IdeApplication *self)
{
  g_autoptr(GDBusConnection) connection = NULL;
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (ide_application_is_worker (self));

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

  g_assert (G_IS_DBUS_CONNECTION (connection));

  engine = peas_engine_get_default ();
  plugin_info = peas_engine_get_plugin_info (engine, self->type);

  if ((plugin_info != NULL) && peas_plugin_info_is_loaded (plugin_info))
    {
      PeasExtension *exten;

      exten = peas_engine_create_extension (engine, plugin_info, IDE_TYPE_WORKER, NULL);

      if (exten != NULL)
        {
          ide_worker_register_service (IDE_WORKER (exten), connection);
          IDE_GOTO (success);
        }
    }

  g_error ("Failed to create \"%s\" worker.", self->type);

  IDE_EXIT;

success:
  g_application_hold (G_APPLICATION (self));
  g_dbus_connection_start_message_processing (connection);

  IDE_EXIT;
}

static void
ide_application_setup_search_paths (void)
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
 * ide_application_make_skeleton_dirs:
 * @self: A #IdeApplication.
 *
 * Creates all the directories we might need later. Simpler to just ensure they
 * are created during startup.
 */
static void
ide_application_make_skeleton_dirs (IdeApplication *self)
{
  gchar *path;

  g_return_if_fail (IDE_IS_APPLICATION (self));

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
}

static void
ide_application_register_theme_overrides (IdeApplication *application)
{
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GtkCssProvider) provider = NULL;
  GtkSettings *gtk_settings;
  GdkScreen *screen;

  IDE_ENTRY;

  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (), "/org/gnome/builder/icons/");

  provider = ide_css_provider_new ();
  screen = gdk_screen_get_default ();
  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_settings = gtk_settings_get_for_screen (screen);
  settings = g_settings_new ("org.gnome.builder");
  g_settings_bind (settings, "night-mode", gtk_settings, "gtk-application-prefer-dark-theme",
                   G_SETTINGS_BIND_DEFAULT);

  IDE_EXIT;
}

static void
ide_application_load_keybindings (IdeApplication *self)
{
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *name = NULL;

  /* TODO: Move this to keybindings */
  static const struct { gchar *name; gchar *binding; } shared_bindings[] = {
    { "workbench.show-left-pane", "F9" },
    { "workbench.show-right-pane", "<shift>F9" },
    { "workbench.show-bottom-pane", "<ctrl>F9" },
    { "workbench.toggle-panels", "<ctrl><shift>F9" },
    { "workbench.focus-left", "<ctrl>grave" },
    { "workbench.focus-right", "<ctrl>9" },
    { "workbench.focus-stack(1)", "<ctrl>1" },
    { "workbench.focus-stack(2)", "<ctrl>2" },
    { "workbench.focus-stack(3)", "<ctrl>3" },
    { "workbench.focus-stack(4)", "<ctrl>4" },
    { "workbench.focus-stack(5)", "<ctrl>5" },
    { "workbench.show-gear-menu", "F10" },
    { "workbench.global-search", "<ctrl>period" },
    { "app.preferences", "<Primary>comma" },
    { "app.shortcuts", "<ctrl>question" },
    { "workbench.new-document", "<ctrl>n" },
    { "workbench.open-document", "<ctrl>o" },
    { NULL }
  };
  gsize i;

  g_assert (IDE_IS_APPLICATION (self));

  settings = g_settings_new ("org.gnome.builder.editor");
  name = g_settings_get_string (settings, "keybindings");
  self->keybindings = ide_keybindings_new (GTK_APPLICATION (self), name);
  g_settings_bind (settings, "keybindings", self->keybindings, "mode", G_SETTINGS_BIND_GET);

  for (i = 0; shared_bindings [i].name; i++)
    {
      const gchar *accels[2] = { shared_bindings [i].binding, NULL };
      gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                             shared_bindings [i].name,
                                             accels);
    }
}

static IdeWorkbench *
ide_application_find_workbench_for_file (IdeApplication *self,
                                        GFile         *file)
{
  GList *iter;
  GList *workbenches;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (G_IS_FILE (file));

  workbenches = gtk_application_get_windows (GTK_APPLICATION (self));

  /*
   * Find the a project that contains this file in its working directory.
   */
  for (iter = workbenches; iter; iter = iter->next)
    {
      if (IDE_IS_WORKBENCH (iter->data))
        {
          IdeWorkbench *workbench = iter->data;
          g_autofree gchar *relpath = NULL;
          IdeContext *context;
          IdeVcs *vcs;
          GFile *workdir;

          context = ide_workbench_get_context (workbench);
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
    if (IDE_IS_WORKBENCH (iter->data))
      return iter->data;

  return NULL;
}

static void
ide_application__context_new_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeContext) context = NULL;
  IdeApplication *self;
  IdeWorkbench *workbench;
  GPtrArray *ar;
  GError *error = NULL;
  gsize i;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  ar = g_task_get_task_data (task);

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (ar);

  context = ide_context_new_finish (result, &error);

  if (!context)
    {
      g_task_return_error (task, error);
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

  workbench = g_object_new (IDE_TYPE_WORKBENCH,
                            "application", self,
                            "context", context,
                            NULL);

  for (i = 0; i < ar->len; i++)
    {
      GFile *file;

      file = g_ptr_array_index (ar, i);
      g_assert (G_IS_FILE (file));

      //ide_workbench_open (workbench, file);
    }

  gtk_window_present (GTK_WINDOW (workbench));

  g_task_return_boolean (task, TRUE);

cleanup:
  g_application_unmark_busy (G_APPLICATION (self));
  g_application_release (G_APPLICATION (self));
}

/**
 * ide_application_open_project_async:
 * @self: A #IdeApplication.
 * @file: A #GFile.
 * @additional_files: (element-type GFile) (nullable): A #GPtrArray of #GFile or %NULL.
 *
 */
void
ide_application_open_project_async (IdeApplication      *self,
                                    GFile               *file,
                                    GPtrArray           *additional_files,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GFile) directory = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  GList *windows;
  GList *iter;

  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (iter = windows; iter; iter = iter->next)
    {
      if (IDE_IS_WORKBENCH (iter->data))
        {
          IdeContext *context;

          context = ide_workbench_get_context (iter->data);

          if (context != NULL)
            {
              GFile *project_file;

              project_file = ide_context_get_project_file (context);

              if (g_file_equal (file, project_file))
                {
                  gtk_window_present (iter->data);
                  g_task_return_boolean (task, TRUE);
                  return;
                }
            }
        }
    }

  if (additional_files)
    ar = g_ptr_array_ref (additional_files);
  else
    ar = g_ptr_array_new ();

  g_task_set_task_data (task, g_ptr_array_ref (ar), (GDestroyNotify)g_ptr_array_unref);

  if (g_file_query_file_type (file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
    directory = g_object_ref (file);
  else
    directory = g_file_get_parent (file);

  g_application_mark_busy (G_APPLICATION (self));
  g_application_hold (G_APPLICATION (self));

  ide_context_new_async (directory,
                         NULL,
                         ide_application__context_new_cb,
                         g_object_ref (task));
}

gboolean
ide_application_open_project_finish (IdeApplication  *self,
                                     GAsyncResult    *result,
                                     GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_APPLICATION (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
ide_application_open (GApplication  *application,
                      GFile        **files,
                      gint           n_files,
                      const gchar   *hint)
{
  IdeApplication *self = (IdeApplication *)application;
  IdeWorkbench *workbench;
  g_autoptr(GPtrArray) ar = NULL;
  guint i;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  /*
   * Try to open the files using an existing workbench.
   */
  for (i = 0; i < n_files; i++)
    {
      GFile *file = files [i];

      g_assert (G_IS_FILE (file));

      workbench = ide_application_find_workbench_for_file (self, file);

      if (workbench != NULL)
        {
          //ide_workbench_open (workbench, file);
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
      GFile *file = g_ptr_array_index (ar, 0);

      ide_application_open_project_async (self, file, ar, NULL, NULL, NULL);
    }

  IDE_EXIT;
}

void
ide_application_show_projects_window (IdeApplication *self)
{
#if 0
  IdeProjectsDialog *window;
  GList *windows;

  g_assert (IDE_IS_APPLICATION (self));

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (; windows; windows = windows->next)
    {
      if (IDE_IS_GREETER_WINDOW (windows->data))
        {
          gtk_window_present (windows->data);
          return;
        }
    }

  if (self->recent_projects == NULL)
    {
      self->recent_projects = ide_recent_projects_new ();
      ide_recent_projects_discover_async (self->recent_projects, NULL, NULL, NULL);
    }

  window = g_object_new (IDE_TYPE_GREETER_WINDOW,
                         "application", self,
                         "recent-projects", self->recent_projects,
                         NULL);
  gtk_window_group_add_window (self->greeter_group, GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (window));
#endif
}

static void
ide_application_activate (GApplication *application)
{
  IdeApplication *self = (IdeApplication *)application;
  IdeWorkbench *workbench;
  GList *list;

  g_assert (IDE_IS_APPLICATION (self));

  if (ide_application_is_worker (self))
    {
      ide_application_load_worker (self);
      return;
    }

  list = gtk_application_get_windows (GTK_APPLICATION (application));

  for (; list; list = list->next)
    {
      if (IDE_IS_WORKBENCH (list->data))
        {
          gtk_window_present (GTK_WINDOW (list->data));
          return;
        }
    }

  workbench = g_object_new (IDE_TYPE_WORKBENCH,
                            "application", self,
                            NULL);
  gtk_window_maximize (GTK_WINDOW (workbench));
  gtk_window_present (GTK_WINDOW (workbench));
}

static void
ide_application__extension_added (PeasExtensionSet    *extensions,
                                  PeasPluginInfo      *plugin_info,
                                  IdeApplicationAddin *addin,
                                  IdeApplication      *self)
{
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (PEAS_IS_EXTENSION_SET (extensions));

  ide_application_addin_load (addin, self);
}

static void
ide_application__extension_removed (PeasExtensionSet    *extensions,
                                    PeasPluginInfo      *plugin_info,
                                    IdeApplicationAddin *addin,
                                    IdeApplication      *self)
{
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (addin));
  g_assert (PEAS_IS_EXTENSION_SET (extensions));

  ide_application_addin_unload (addin, self);
}

static void
ide_application_load_addins (IdeApplication *self)
{
  PeasEngine *engine;

  g_assert (IDE_IS_APPLICATION (self));

  engine = peas_engine_get_default ();

  self->extensions = peas_extension_set_new (engine, IDE_TYPE_APPLICATION_ADDIN, NULL);

  peas_extension_set_foreach (self->extensions,
                              (PeasExtensionSetForeachFunc)ide_application__extension_added,
                              self);

  g_signal_connect_object (self->extensions,
                           "extension-added",
                           G_CALLBACK (ide_application__extension_added),
                           self,
                           0);

  g_signal_connect_object (self->extensions,
                           "extension-removed",
                           G_CALLBACK (ide_application__extension_removed),
                           self,
                           0);
}

static void
ide_application_startup (GApplication *app)
{
  IdeApplication *self = (IdeApplication *)app;
  GgitFeatureFlags ggit_flags;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  self->startup_time = g_date_time_new_now_utc ();

  g_resources_register (ide_get_resource ());

  g_application_set_resource_base_path (app, "/org/gnome/builder");

  g_irepository_prepend_search_path (PACKAGE_LIBDIR"/gnome-builder/girepository-1.0");

  if (!ide_application_is_worker (self))
    self->greeter_group = gtk_window_group_new ();

  _ide_battery_monitor_init ();
  _ide_thread_pool_init (ide_application_is_worker (self));

  modeline_parser_init ();

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

  G_APPLICATION_CLASS (ide_application_parent_class)->startup (app);

  if (!ide_application_is_worker (self))
    {
      ide_application_make_skeleton_dirs (self);
      ide_application_actions_init (self);
      ide_application_register_theme_overrides (self);
      ide_application_setup_search_paths ();
      ide_application_load_keybindings (self);
      ide_application_load_plugins (self);
      ide_application_load_addins (self);
    }

  IDE_EXIT;
}

static gboolean
ide_application_increase_verbosity (void)
{
  ide_log_increase_verbosity ();
  return TRUE;
}

static gint
ide_application_handle_local_options (GApplication *app,
                                      GVariantDict *options)
{
  if (g_variant_dict_contains (options, "version"))
    {
      g_print ("%s - Version %s\n", g_get_application_name (), VERSION);
      return 0;
    }

   if (g_variant_dict_contains (options, "standalone") || g_variant_dict_contains (options, "type"))
    {
      GApplicationFlags flags;

      flags = g_application_get_flags (app);
      g_application_set_flags (app, flags | G_APPLICATION_NON_UNIQUE);
    }

  return -1;
}

static gboolean
ide_application_local_command_line (GApplication   *application,
                                    gchar        ***arguments,
                                    int            *exit_status)
{
  IdeApplication *self = (IdeApplication *)application;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (arguments != NULL);
  g_assert (*arguments != NULL);
  g_assert (exit_status != NULL);

  self->argv0 = g_strdup ((*arguments) [0]);

  return G_APPLICATION_CLASS (ide_application_parent_class)->
    local_command_line (application, arguments, exit_status);
}

static void
ide_application_finalize (GObject *object)
{
  IdeApplication *self = (IdeApplication *)object;

  IDE_ENTRY;

  g_clear_object (&self->extensions);
  g_clear_pointer (&self->startup_time, g_date_time_unref);
  g_clear_pointer (&self->argv0, g_free);
  g_clear_object (&self->keybindings);
  g_clear_object (&self->recent_projects);
  g_clear_object (&self->greeter_group);

  G_OBJECT_CLASS (ide_application_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_application_class_init (IdeApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  IDE_ENTRY;

  object_class->finalize = ide_application_finalize;

  app_class->activate = ide_application_activate;
  app_class->startup = ide_application_startup;
  app_class->open = ide_application_open;
  app_class->local_command_line = ide_application_local_command_line;
  app_class->handle_local_options = ide_application_handle_local_options;

  IDE_EXIT;
}

static void
ide_application_init (IdeApplication *app)
{
  GOptionEntry options[] = {
    { "standalone",
      's',
      G_OPTION_FLAG_IN_MAIN,
      G_OPTION_ARG_NONE,
      NULL,
      N_("Run Builder in standalone mode") },

    { "version",
      0,
      G_OPTION_FLAG_IN_MAIN,
      G_OPTION_ARG_NONE,
      NULL,
      N_("Show the application's version") },

    { "verbose",
      'v',
      G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_CALLBACK,
      ide_application_increase_verbosity,
      N_("Increase verbosity. May be specified multiple times.") },

    { "dbus-address",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_STRING,
      &app->dbus_address,
      N_("The DBus server address for which to connect.") },

    { "type",
      0,
      G_OPTION_FLAG_HIDDEN,
      G_OPTION_ARG_STRING,
      &app->type,
      N_("The type of plugin worker process to run.") },

    { NULL }
  };

  IDE_ENTRY;

  g_application_add_main_option_entries (G_APPLICATION (app), options);

  IDE_EXIT;
}

GDateTime *
ide_application_get_startup_time (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  return self->startup_time;
}

const gchar *
ide_application_get_keybindings_mode (IdeApplication *self)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  return ide_keybindings_get_mode (self->keybindings);
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

  if (self->worker_manager == NULL)
    self->worker_manager = ide_worker_manager_new (self->argv0);

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

  if (self->recent_projects == NULL)
    {
      self->recent_projects = ide_recent_projects_new ();
      ide_recent_projects_discover_async (self->recent_projects, NULL, NULL, NULL);
    }

  return self->recent_projects;
}
