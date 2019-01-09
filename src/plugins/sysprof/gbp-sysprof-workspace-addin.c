/* gbp-sysprof-workspace-addin.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>
#include <sysprof.h>

#include "gbp-sysprof-surface.h"
#include "gbp-sysprof-workspace-addin.h"

struct _GbpSysprofWorkspaceAddin
{
  GObject                parent_instance;

  GSimpleActionGroup    *actions;
  SpProfiler            *profiler;

  GbpSysprofSurface     *surface;
  IdeWorkspace          *workspace;

  GtkBox                *zoom_controls;
};

static void workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpSysprofWorkspaceAddin, gbp_sysprof_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_sysprof_workspace_addin_update_controls (GbpSysprofWorkspaceAddin *self)
{
  IdeSurface *surface;
  gboolean visible;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));

  if (self->workspace == NULL)
    return;

  surface = ide_workspace_get_visible_surface (self->workspace);
  visible = GBP_IS_SYSPROF_SURFACE (surface) &&
            !!gbp_sysprof_surface_get_reader (GBP_SYSPROF_SURFACE (surface));

  if (self->zoom_controls)
    gtk_widget_set_visible (GTK_WIDGET (self->zoom_controls), visible);
}

static void
profiler_stopped (GbpSysprofWorkspaceAddin *self,
                  SpProfiler               *profiler)
{
  g_autoptr(SpCaptureReader) reader = NULL;
  g_autoptr(GError) error = NULL;
  SpCaptureWriter *writer;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (SP_IS_PROFILER (profiler));

  if (self->profiler != profiler)
    IDE_EXIT;

  if (self->workspace == NULL)
    IDE_EXIT;

  writer = sp_profiler_get_writer (profiler);
  reader = sp_capture_writer_create_reader (writer, &error);

  if (reader == NULL)
    {
      /* TODO: Propagate error to an infobar or similar */
      g_warning ("%s", error->message);
      IDE_EXIT;
    }

  gbp_sysprof_surface_set_reader (self->surface, reader);

  ide_workspace_set_visible_surface_name (self->workspace, "profiler");

  gbp_sysprof_workspace_addin_update_controls (self);

  IDE_EXIT;
}

static void
profiler_child_spawned (GbpSysprofWorkspaceAddin *self,
                        const gchar              *identifier,
                        IdeRunner                *runner)
{
  GPid pid = 0;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (identifier != NULL);
  g_assert (IDE_IS_RUNNER (runner));

  if (!SP_IS_PROFILER (self->profiler))
    return;

#ifdef G_OS_UNIX
  pid = g_ascii_strtoll (identifier, NULL, 10);
#endif

  if G_UNLIKELY (pid == 0)
    {
      g_warning ("Failed to parse integer value from %s", identifier);
      return;
    }

  IDE_TRACE_MSG ("Adding pid %s to profiler", identifier);

  sp_profiler_add_pid (self->profiler, pid);
  sp_profiler_start (self->profiler);
}

static gchar *
get_runtime_sysroot (IdeContext  *context,
                     const gchar *path)
{
  IdeConfigurationManager *config_manager;
  IdeConfiguration *config;
  IdeRuntime *runtime;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONTEXT (context));

  config_manager = ide_configuration_manager_from_context (context);
  config = ide_configuration_manager_get_current (config_manager);
  runtime = ide_configuration_get_runtime (config);

  if (runtime != NULL)
    {
      g_autoptr(GFile) base = g_file_new_for_path (path);
      g_autoptr(GFile) translated = ide_runtime_translate_file (runtime, base);

      if (translated != NULL)
        return g_file_get_path (translated);
    }

  return NULL;
}

static void
profiler_run_handler (IdeRunManager *run_manager,
                      IdeRunner     *runner,
                      gpointer       user_data)
{
  GbpSysprofWorkspaceAddin *self = user_data;
  g_autoptr(SpSource) proc_source = NULL;
  g_autoptr(SpSource) perf_source = NULL;
  g_autoptr(SpSource) hostinfo_source = NULL;
  g_autoptr(SpSource) memory_source = NULL;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  if (SP_IS_PROFILER (self->profiler))
    {
      if (sp_profiler_get_is_running (self->profiler))
        sp_profiler_stop (self->profiler);
      g_clear_object (&self->profiler);
    }

  /*
   * First get a copy of the active runtime and find the root of it's
   * translation path. That way we can adjust for the sysroot when
   * resolving symbols.
   *
   * TODO: Hardcoding /usr and /app here sucks, we need a way to have
   *       this in the flatpak plugin instead (and associated plumbing
   *       to abstract it). We probably should just have a "get_debug_paths"
   *       type helper from the runtime.
   */
  {
    /* Put debug directories first so the resolve higher */
    static const gchar *dirs[] = {
      "/app/lib/debug",
      "/usr/lib/debug",
      "/app/bin",
      "/app/lib",
      "/usr/lib",
      NULL
    };

    context = ide_object_get_context (IDE_OBJECT (run_manager));

    for (guint i = 0; dirs[i]; i++)
      {
        g_autofree gchar *path = get_runtime_sysroot (context, dirs[i]);

        if (path != NULL)
          sp_symbol_dirs_add (path);
      }
  }

  self->profiler = sp_local_profiler_new ();

  g_signal_connect_object (self->profiler,
                           "stopped",
                           G_CALLBACK (gbp_sysprof_workspace_addin_update_controls),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_widget_hide (GTK_WIDGET (self->zoom_controls));

  /*
   * Currently we require whole-system because otherwise we can get a situation
   * where we only watch the spawning process (say jhbuild, flatpak, etc).
   * Longer term we either need a way to follow-children and/or limit to a
   * cgroup/process-group.
   */
  sp_profiler_set_whole_system (SP_PROFILER (self->profiler), TRUE);

  proc_source = sp_proc_source_new ();
  sp_profiler_add_source (self->profiler, proc_source);

  perf_source = sp_perf_source_new ();
  sp_profiler_add_source (self->profiler, perf_source);

  hostinfo_source = sp_hostinfo_source_new ();
  sp_profiler_add_source (self->profiler, hostinfo_source);

  memory_source = sp_memory_source_new ();
  sp_profiler_add_source (self->profiler, memory_source);

  /*
   * TODO:
   *
   * We need to synchronize the inferior with the parent here. Ideally, we would
   * prepend the application launch (to some degree) with the application we want
   * to execute. In this case, we might want to add a "gnome-builder-sysprof"
   * helper that will synchronize with the parent, and then block until we start
   * the process (with the appropriate pid) before exec() otherwise we could
   * miss the exit of the app and race to add the pid to the profiler.
   */

  g_signal_connect_object (runner,
                           "spawned",
                           G_CALLBACK (profiler_child_spawned),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->profiler,
                           "stopped",
                           G_CALLBACK (profiler_stopped),
                           self,
                           G_CONNECT_SWAPPED);

  gbp_sysprof_surface_set_profiler (self->surface, self->profiler);

  ide_workspace_set_visible_surface (self->workspace, IDE_SURFACE (self->surface));
}

static void
gbp_sysprof_workspace_addin_open_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GbpSysprofWorkspaceAddin *self = (GbpSysprofWorkspaceAddin *)object;
  g_autoptr(SpCaptureReader) reader = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  reader = ide_task_propagate_pointer (IDE_TASK (result), &error);

  g_assert (reader || error != NULL);

  if (reader == NULL)
    {
      g_message ("%s", error->message);
      return;
    }

  gbp_sysprof_surface_set_profiler (self->surface, NULL);
  gbp_sysprof_surface_set_reader (self->surface, reader);

  gbp_sysprof_workspace_addin_update_controls (self);
}

static void
gbp_sysprof_workspace_addin_open_worker (IdeTask      *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GError) error = NULL;
  SpCaptureReader *reader;
  GFile *file = task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (source_object));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  path = g_file_get_path (file);

  if (NULL == (reader = sp_capture_reader_new (path, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, reader, (GDestroyNotify)sp_capture_reader_unref);
}

static void
gbp_sysprof_workspace_addin_open (GbpSysprofWorkspaceAddin *self,
                                  GFile                    *file)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (G_IS_FILE (file));

  if (!g_file_is_native (file))
    {
      g_warning ("Can only open local sysprof capture files.");
      return;
    }

  task = ide_task_new (self, NULL, gbp_sysprof_workspace_addin_open_cb, NULL);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);
  ide_task_run_in_thread (task, gbp_sysprof_workspace_addin_open_worker);
}

static void
open_profile_action (GSimpleAction *action,
                     GVariant      *variant,
                     gpointer       user_data)
{
  GbpSysprofWorkspaceAddin *self = user_data;
  g_autoptr(GFile) workdir = NULL;
  GtkFileChooserNative *native;
  GtkFileFilter *filter;
  IdeContext *context;
  gint ret;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (GBP_IS_SYSPROF_SURFACE (self->surface));

  ide_workspace_set_visible_surface (self->workspace, IDE_SURFACE (self->surface));

  context = ide_workspace_get_context (self->workspace);
  workdir = ide_context_ref_workdir (context);

  native = gtk_file_chooser_native_new (_("Open Sysprof Capture…"),
                                        GTK_WINDOW (self->workspace),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Open"),
                                        _("Cancel"));
  gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (native), workdir, NULL);

  /* Add our filter for sysprof capture files.  */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Sysprof Capture (*.syscap)"));
  gtk_file_filter_add_pattern (filter, "*.syscap");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);

  /* And all files now */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);

  /* Unlike gtk_dialog_run(), this will handle processing
   * various I/O events and so should be safe to use.
   */
  ret = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));

  if (ret == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = NULL;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));
      if (G_IS_FILE (file))
        gbp_sysprof_workspace_addin_open (self, file);
    }

  gtk_native_dialog_hide (GTK_NATIVE_DIALOG (native));
  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
}

static void
gbp_sysprof_workspace_addin_finalize (GObject *object)
{
  GbpSysprofWorkspaceAddin *self = (GbpSysprofWorkspaceAddin *)object;

  g_assert (IDE_IS_MAIN_THREAD ());

  g_clear_object (&self->actions);

  G_OBJECT_CLASS (gbp_sysprof_workspace_addin_parent_class)->finalize (object);
}

static void
gbp_sysprof_workspace_addin_class_init (GbpSysprofWorkspaceAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_sysprof_workspace_addin_finalize;
}

static void
gbp_sysprof_workspace_addin_init (GbpSysprofWorkspaceAddin *self)
{
  static const GActionEntry entries[] = {
    { "open-profile", open_profile_action },
  };

  g_assert (IDE_IS_MAIN_THREAD ());

  self->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
}

static void
run_manager_stopped (GbpSysprofWorkspaceAddin *self,
                     IdeRunManager            *run_manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  if (self->profiler != NULL && sp_profiler_get_is_running (self->profiler))
    sp_profiler_stop (self->profiler);
}

static gboolean
zoom_level_to_string (GBinding     *binding,
                      const GValue *from_value,
                      GValue       *to_value,
                      gpointer      user_data)
{
  gdouble level = g_value_get_double (from_value);
  g_value_take_string (to_value, g_strdup_printf ("%d%%", (gint)(level * 100.0)));
  return TRUE;
}

static void
gbp_sysprof_workspace_addin_load (IdeWorkspaceAddin *addin,
                                  IdeWorkspace      *workspace)
{
  GbpSysprofWorkspaceAddin *self = (GbpSysprofWorkspaceAddin *)addin;
  SpZoomManager *zoom_manager;
  IdeRunManager *run_manager;
  IdeHeaderBar *header;
  IdeContext *context;
  GtkLabel *label;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  self->workspace = workspace;

  context = ide_workspace_get_context (workspace);

  /*
   * Register our custom run handler to activate the profiler.
   */
  run_manager = ide_run_manager_from_context (context);
  ide_run_manager_add_handler (run_manager,
                               "profiler",
                               _("Run with Profiler"),
                               "utilities-system-monitor-symbolic",
                               "<primary>F8",
                               profiler_run_handler,
                               self,
                               NULL);
  g_signal_connect_object (run_manager,
                           "stopped",
                           G_CALLBACK (run_manager_stopped),
                           self,
                           G_CONNECT_SWAPPED);

  /* Add the surface to the workspace. */
  self->surface = g_object_new (GBP_TYPE_SYSPROF_SURFACE,
                                "visible", TRUE,
                                NULL);
  g_signal_connect (self->surface,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->surface);
  ide_workspace_add_surface (workspace, IDE_SURFACE (self->surface));

  zoom_manager = gbp_sysprof_surface_get_zoom_manager (self->surface);

  /*
   * Add our actions to the workspace so they can be activated via the
   * headerbar or the surface.
   */
  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "profiler", G_ACTION_GROUP (self->actions));
  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "profiler-zoom", G_ACTION_GROUP (zoom_manager));

  /* Add our buttons to the header. */
  header = ide_workspace_get_header_bar (workspace);
  self->zoom_controls = g_object_new (GTK_TYPE_BOX,
                                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                                      NULL);
  g_signal_connect (self->zoom_controls,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->zoom_controls);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->zoom_controls), "linked");
  gtk_container_add (GTK_CONTAINER (self->zoom_controls),
                     g_object_new (GTK_TYPE_BUTTON,
                                   "action-name", "profiler-zoom.zoom-out",
                                   "can-focus", FALSE,
                                   "child", g_object_new (GTK_TYPE_IMAGE,
                                                          "icon-name", "zoom-out-symbolic",
                                                          "visible", TRUE,
                                                          NULL),
                                   "visible", TRUE,
                                   NULL));
  label = g_object_new (GTK_TYPE_LABEL,
                        "width-chars", 5,
                        "visible", TRUE,
                        NULL);
  g_object_bind_property_full (zoom_manager, "zoom", label, "label", G_BINDING_SYNC_CREATE,
                               zoom_level_to_string, NULL, NULL, NULL);
  gtk_container_add (GTK_CONTAINER (self->zoom_controls),
                     g_object_new (GTK_TYPE_BUTTON,
                                   "action-name", "profiler-zoom.zoom-one",
                                   "can-focus", FALSE,
                                   "child", label,
                                   "visible", TRUE,
                                   NULL));
  gtk_container_add (GTK_CONTAINER (self->zoom_controls),
                     g_object_new (GTK_TYPE_BUTTON,
                                   "action-name", "profiler-zoom.zoom-in",
                                   "can-focus", FALSE,
                                   "child", g_object_new (GTK_TYPE_IMAGE,
                                                          "icon-name", "zoom-in-symbolic",
                                                          "visible", TRUE,
                                                          NULL),
                                   "visible", TRUE,
                                   NULL));
  ide_header_bar_add_primary (header, GTK_WIDGET (self->zoom_controls));
}

static void
gbp_sysprof_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                    IdeWorkspace      *workspace)
{
  GbpSysprofWorkspaceAddin *self = (GbpSysprofWorkspaceAddin *)addin;
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  context = ide_workspace_get_context (workspace);

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "profiler", NULL);
  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "profiler-zoom", NULL);

  run_manager = ide_run_manager_from_context (context);
  ide_run_manager_remove_handler (run_manager, "profiler");

  if (self->surface)
    gtk_widget_destroy (GTK_WIDGET (self->surface));

  if (self->zoom_controls)
    gtk_widget_destroy (GTK_WIDGET (self->zoom_controls));

  self->zoom_controls = NULL;
  self->surface = NULL;
  self->workspace = NULL;
}

static void
gbp_sysprof_workspace_addin_surface_set (IdeWorkspaceAddin *addin,
                                         IdeSurface        *surface)
{
  GbpSysprofWorkspaceAddin *self = (GbpSysprofWorkspaceAddin *)addin;

  g_assert (IDE_IS_WORKSPACE_ADDIN (addin));
  g_assert (!surface || IDE_IS_SURFACE (surface));

  gbp_sysprof_workspace_addin_update_controls (self);
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_sysprof_workspace_addin_load;
  iface->unload = gbp_sysprof_workspace_addin_unload;
  iface->surface_set = gbp_sysprof_workspace_addin_surface_set;
}
