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

#include "gb-application.h"
#include "gb-application-actions.h"
#include "gb-application-addin.h"
#include "gb-application-private.h"
#include "gb-css-provider.h"
#include "gb-editor-document.h"
#include "gb-glib.h"
#include "gb-greeter-window.h"
#include "gb-projects-dialog.h"
#include "gb-resources.h"
#include "gb-workbench.h"

G_DEFINE_TYPE (GbApplication, gb_application, GTK_TYPE_APPLICATION)

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

static void
gb_application_register_theme_overrides (GbApplication *application)
{
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GtkCssProvider) provider = NULL;
  GtkSettings *gtk_settings;
  GdkScreen *screen;

  IDE_ENTRY;

  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                                    "/org/gnome/builder/icons/");

  provider = gb_css_provider_new ();
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
gb_application_load_keybindings (GbApplication *self)
{
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *name = NULL;
  static const struct { gchar *name; gchar *binding; } shared_bindings[] = {
    { "workbench.show-left-pane", "F9" },
    { "workbench.show-right-pane", "<shift>F9" },
    { "workbench.show-bottom-pane", "<ctrl>F9" },
    { "workbench.toggle-panels", "<ctrl><shift>F9" },
    { "workspace.focus-sidebar", "<ctrl>0" },
    { "workbench.focus-stack(1)", "<ctrl>1" },
    { "workbench.focus-stack(2)", "<ctrl>2" },
    { "workbench.focus-stack(3)", "<ctrl>3" },
    { "workbench.focus-stack(4)", "<ctrl>4" },
    { "workbench.focus-stack(5)", "<ctrl>5" },
    { "workbench.show-gear-menu", "F10" },
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
  IdeBuffer *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (IDE_IS_FILE (file));

  ret = g_object_new (GB_TYPE_EDITOR_DOCUMENT,
                      "context", ide_object_get_context (IDE_OBJECT (buffer_manager)),
                      "file", file,
                      "highlight-diagnostics", TRUE,
                      NULL);

  IDE_RETURN (ret);
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

  bufmgr = ide_context_get_buffer_manager (context);
  g_signal_connect (bufmgr, "create-buffer", G_CALLBACK (on_create_buffer), NULL);

  workbench = g_object_new (GB_TYPE_WORKBENCH,
                            "application", self,
                            "context", context,
                            NULL);

  for (i = 0; i < ar->len; i++)
    {
      GFile *file;

      file = g_ptr_array_index (ar, i);
      g_assert (G_IS_FILE (file));

      gb_workbench_open (workbench, file);
    }

  gtk_window_present (GTK_WINDOW (workbench));

  g_task_return_boolean (task, TRUE);

cleanup:
  g_application_unmark_busy (G_APPLICATION (self));
  g_application_release (G_APPLICATION (self));
}

void
gb_application_open_project_async (GbApplication       *self,
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

  g_return_if_fail (GB_IS_APPLICATION (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (iter = windows; iter; iter = iter->next)
    {
      if (GB_IS_WORKBENCH (iter->data))
        {
          IdeContext *context;

          context = gb_workbench_get_context (iter->data);

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
                         gb_application__context_new_cb,
                         g_object_ref (task));
}

gboolean
gb_application_open_project_finish (GbApplication  *self,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (GB_IS_APPLICATION (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
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
      GFile *file = g_ptr_array_index (ar, 0);

      gb_application_open_project_async (self, file, ar, NULL, NULL, NULL);
    }

  IDE_EXIT;
}

void
gb_application_show_projects_window (GbApplication *self)
{
  GbProjectsDialog *window;
  GList *windows;

  g_assert (GB_IS_APPLICATION (self));

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (; windows; windows = windows->next)
    {
      if (GB_IS_GREETER_WINDOW (windows->data))
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

  window = g_object_new (GB_TYPE_GREETER_WINDOW,
                         "application", self,
                         "recent-projects", self->recent_projects,
                         NULL);
  gtk_window_group_add_window (self->greeter_group, GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (window));
}

static void
gb_application_activate (GApplication *application)
{
  GbApplication *self = (GbApplication *)application;

  g_assert (GB_IS_APPLICATION (self));

  gb_application_show_projects_window (self);
}

static void
gb_application__extension_added (PeasExtensionSet   *extensions,
                                 PeasPluginInfo     *plugin_info,
                                 GbApplicationAddin *addin,
                                 GbApplication      *self)
{
  g_assert (GB_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (GB_IS_APPLICATION_ADDIN (addin));
  g_assert (PEAS_IS_EXTENSION_SET (extensions));

  gb_application_addin_load (addin, self);
}

static void
gb_application__extension_removed (PeasExtensionSet   *extensions,
                                   PeasPluginInfo     *plugin_info,
                                   GbApplicationAddin *addin,
                                   GbApplication      *self)
{
  g_assert (GB_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);
  g_assert (GB_IS_APPLICATION_ADDIN (addin));
  g_assert (PEAS_IS_EXTENSION_SET (extensions));

  gb_application_addin_unload (addin, self);
}

static void
gb_application_load_extensions (GbApplication *self)
{
  PeasEngine *engine;

  g_assert (GB_IS_APPLICATION (self));

  engine = peas_engine_get_default ();

  self->extensions = peas_extension_set_new (engine, GB_TYPE_APPLICATION_ADDIN, NULL);

  peas_extension_set_foreach (self->extensions,
                              (PeasExtensionSetForeachFunc)gb_application__extension_added,
                              self);

  g_signal_connect_object (self->extensions,
                           "extension-added",
                           G_CALLBACK (gb_application__extension_added),
                           self,
                           0);

  g_signal_connect_object (self->extensions,
                           "extension-removed",
                           G_CALLBACK (gb_application__extension_removed),
                           self,
                           0);
}

static void
gb_application_startup (GApplication *app)
{
  GbApplication *self = (GbApplication *)app;

  IDE_ENTRY;

  g_assert (GB_IS_APPLICATION (self));

  self->started_at = g_date_time_new_now_utc ();
  self->greeter_group = gtk_window_group_new ();

  g_resources_register (gb_get_resource ());
  g_application_set_resource_base_path (app, "/org/gnome/builder");

  G_APPLICATION_CLASS (gb_application_parent_class)->startup (app);

  gb_application_make_skeleton_dirs (self);
  gb_application_actions_init (self);
  gb_application_register_theme_overrides (self);
  gb_application_setup_search_paths ();
  gb_application_load_keybindings (self);
  gb_application_load_extensions (self);

  IDE_EXIT;
}

static gboolean
gb_application_increase_verbosity (void)
{
  ide_log_increase_verbosity ();
  return TRUE;
}

static gint
gb_application_handle_local_options (GApplication *app,
                                     GVariantDict *options)
{
  if (g_variant_dict_contains (options, "version"))
    {
      g_print ("%s - Version %s\n", g_get_application_name (), VERSION);
      return 0;
    }

   if (g_variant_dict_contains (options, "standalone"))
    {
      GApplicationFlags flags;

      flags = g_application_get_flags (app);
      g_application_set_flags (app, flags | G_APPLICATION_NON_UNIQUE);
    }

  return -1;
}

static void
gb_application_finalize (GObject *object)
{
  GbApplication *self = (GbApplication *)object;

  IDE_ENTRY;

  g_clear_object (&self->extensions);
  g_clear_pointer (&self->started_at, g_date_time_unref);
  g_clear_object (&self->keybindings);
  g_clear_object (&self->recent_projects);
  g_clear_object (&self->greeter_group);

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
  app_class->handle_local_options = gb_application_handle_local_options;

  IDE_EXIT;
}

static void
gb_application_init (GbApplication *app)
{
  static const GOptionEntry options[] = {
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
      gb_application_increase_verbosity,
      N_("Increase verbosity. May be specified multiple times.") },
    { NULL }
  };

  IDE_ENTRY;

  g_application_add_main_option_entries (G_APPLICATION (app), options);

  IDE_EXIT;
}

GDateTime *
gb_application_get_started_at (GbApplication *self)
{
  g_return_val_if_fail (GB_IS_APPLICATION (self), NULL);

  return self->started_at;
}
