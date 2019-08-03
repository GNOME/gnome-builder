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

#define G_LOG_DOMAIN "gbp-sysprof-workspace-addin"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <sysprof-ui.h>

#include "gbp-sysprof-surface.h"
#include "gbp-sysprof-workspace-addin.h"

struct _GbpSysprofWorkspaceAddin
{
  GObject                parent_instance;

  GSimpleActionGroup    *actions;

  GbpSysprofSurface     *surface;
  IdeWorkspace          *workspace;
};

static void workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpSysprofWorkspaceAddin, gbp_sysprof_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
profiler_child_spawned (IdeRunner       *runner,
                        const gchar     *identifier,
                        SysprofProfiler *profiler)
{
#ifdef G_OS_UNIX
  GPid pid = 0;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (SYSPROF_IS_PROFILER (profiler));
  g_assert (identifier != NULL);
  g_assert (IDE_IS_RUNNER (runner));

  pid = g_ascii_strtoll (identifier, NULL, 10);

  if (pid == 0)
    {
      g_warning ("Failed to parse integer value from %s", identifier);
      return;
    }

  IDE_TRACE_MSG ("Adding pid %s to profiler", identifier);

  sysprof_profiler_add_pid (profiler, pid);
  sysprof_profiler_start (profiler);
#endif
}

static void
runner_exited_cb (IdeRunner       *runner,
                  SysprofProfiler *profiler)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (SYSPROF_IS_PROFILER (profiler));

  if (sysprof_profiler_get_is_running (profiler))
    sysprof_profiler_stop (profiler);
}

static void
foreach_fd (gint     dest_fd,
            gint     fd,
            gpointer user_data)
{
  IdeRunner *runner = user_data;

  g_assert (IDE_IS_RUNNER (runner));
  g_assert (dest_fd >= 0);
  g_assert (fd >= 0);

  ide_runner_take_fd (runner, dup (fd), dest_fd);
}

static void
profiler_run_handler (IdeRunManager *run_manager,
                      IdeRunner     *runner,
                      gpointer       user_data)
{
  GbpSysprofWorkspaceAddin *self = user_data;
  g_autoptr(SysprofProfiler) profiler = NULL;
  g_autoptr(SysprofSource) proc_source = NULL;
  g_autoptr(SysprofSource) perf_source = NULL;
  g_autoptr(SysprofSource) hostinfo_source = NULL;
  g_autoptr(SysprofSource) memory_source = NULL;
  g_autoptr(SysprofSource) app_source = NULL;
  g_autoptr(SysprofSource) gjs_source = NULL;
  g_autoptr(SysprofSource) gtk_source = NULL;
  g_autoptr(SysprofSource) symbols_source = NULL;
  g_autoptr(SysprofSource) netdev_source = NULL;
  g_autoptr(SysprofSource) energy_source = NULL;
  g_autoptr(SysprofSpawnable) spawnable = NULL;
  g_autoptr(GPtrArray) sources = NULL;
  g_auto(GStrv) argv = NULL;
  const gchar * const *env;
  IdeEnvironment *ienv;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  sources = g_ptr_array_new ();

  profiler = sysprof_local_profiler_new ();

  /*
   * Currently we require whole-system because otherwise we can get a situation
   * where we only watch the spawning process (say jhbuild, flatpak, etc).
   * Longer term we either need a way to follow-children and/or limit to a
   * cgroup/process-group.
   */
  sysprof_profiler_set_whole_system (profiler, TRUE);

  proc_source = sysprof_proc_source_new ();
  g_ptr_array_add (sources, proc_source);

  /* TODO: Make this source non-fatal since we have other data collectors */
  perf_source = sysprof_perf_source_new ();
  g_ptr_array_add (sources, perf_source);

  hostinfo_source = sysprof_hostinfo_source_new ();
  g_ptr_array_add (sources, hostinfo_source);

  memory_source = sysprof_memory_source_new ();
  g_ptr_array_add (sources, memory_source);

  gjs_source = sysprof_gjs_source_new ();
  g_ptr_array_add (sources, gjs_source);

  gtk_source = sysprof_tracefd_source_new ();
  sysprof_tracefd_source_set_envvar (SYSPROF_TRACEFD_SOURCE (gtk_source), "GTK_TRACE_FD");
  g_ptr_array_add (sources, gtk_source);

  /* Allow the app to submit us data if it supports "SYSPROF_TRACE_FD" */
  app_source = sysprof_tracefd_source_new ();
  sysprof_tracefd_source_set_envvar (SYSPROF_TRACEFD_SOURCE (app_source), "SYSPROF_TRACE_FD");
  g_ptr_array_add (sources, app_source);

  symbols_source = sysprof_symbols_source_new ();
  g_ptr_array_add (sources, symbols_source);

  energy_source = sysprof_proxy_source_new (G_BUS_TYPE_SYSTEM,
                                            "org.gnome.Sysprof3",
                                            "/org/gnome/Sysprof3/RAPL");
  g_ptr_array_add (sources, energy_source);

  netdev_source = sysprof_netdev_source_new ();
  g_ptr_array_add (sources, netdev_source);

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
                           profiler,
                           0);

  g_signal_connect_object (runner,
                           "exited",
                           G_CALLBACK (runner_exited_cb),
                           profiler,
                           0);

  /*
   * We need to allow the sources to modify the execution environment, so copy
   * the environment into the spawnable, modify it, and the propagate back.
   */
  argv = ide_runner_get_argv (runner);
  ienv = ide_runner_get_environment (runner);

  spawnable = sysprof_spawnable_new ();
  sysprof_spawnable_append_args (spawnable, (const gchar * const *)argv);
  sysprof_spawnable_set_starting_fd (spawnable, ide_runner_get_max_fd (runner) + 1);

  for (guint i = 0; i < sources->len; i++)
    {
      SysprofSource *source = g_ptr_array_index (sources, i);

      sysprof_profiler_add_source (profiler, source);
      sysprof_source_modify_spawn (source, spawnable);
    }

  /* TODO: Propagate argv back to runner.
   *
   * Currently this is a non-issue because none of our sources modify argv.
   * So doing it now is just brittle for no benefit.
   */

  if ((env = sysprof_spawnable_get_environ (spawnable)))
    {
      for (guint i = 0; env[i] != NULL; i++)
        {
          g_autofree gchar *key = NULL;
          g_autofree gchar *value = NULL;

          if (ide_environ_parse (env[i], &key, &value))
            ide_environment_setenv (ienv, key, value);
        }
    }

  sysprof_spawnable_foreach_fd (spawnable, foreach_fd, runner);

  gbp_sysprof_surface_add_profiler (self->surface, profiler);

  ide_workspace_set_visible_surface (self->workspace, IDE_SURFACE (self->surface));
}

static void
gbp_sysprof_workspace_addin_open (GbpSysprofWorkspaceAddin *self,
                                  GFile                    *file)
{
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (G_IS_FILE (file));

  if (!g_file_is_native (file))
    g_warning ("Can only open local sysprof capture files.");
  else
    gbp_sysprof_surface_open (self->surface, file);
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
run_cb (GSimpleAction *action,
        GVariant      *param,
        gpointer       user_data)
{
  GbpSysprofWorkspaceAddin *self = user_data;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));

  if (self->workspace != NULL)
    dzl_gtk_widget_action (GTK_WIDGET (self->workspace),
                           "run-manager",
                           "run-with-handler",
                           g_variant_new_string ("profiler"));
}

static void
show_cb (GSimpleAction *action,
         GVariant      *param,
         gpointer       user_data)
{
  GbpSysprofWorkspaceAddin *self = user_data;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));

  if (self->workspace != NULL)
    ide_workspace_set_visible_surface (self->workspace, IDE_SURFACE (self->surface));
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
    { "run", run_cb },
    { "show", show_cb },
  };

  g_assert (IDE_IS_MAIN_THREAD ());

  self->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
}

static void
gbp_sysprof_workspace_addin_check_supported_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  g_autoptr(GbpSysprofWorkspaceAddin) self = user_data;
  g_autoptr(GError) error = NULL;
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  /* Check if we're unloaded */
  if (self->workspace == NULL)
    return;

  if (!sysprof_check_supported_finish (result, &error))
    {
      g_warning ("Sysprof-3 is not supported, will not enable profiler: %s",
                 error->message);
      return;
    }

  gtk_widget_insert_action_group (GTK_WIDGET (self->workspace),
                                  "profiler",
                                  G_ACTION_GROUP (self->actions));

  /* Register our custom run handler to activate the profiler. */
  context = ide_workspace_get_context (self->workspace);
  run_manager = ide_run_manager_from_context (context);
  ide_run_manager_add_handler (run_manager,
                               "profiler",
                               _("Run with Profiler"),
                               "org.gnome.Sysprof-symbolic",
                               "<primary>F8",
                               profiler_run_handler,
                               self,
                               NULL);

  /* Add the surface to the workspace. */
  self->surface = g_object_new (GBP_TYPE_SYSPROF_SURFACE,
                                "visible", TRUE,
                                NULL);
  g_signal_connect (self->surface,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->surface);
  ide_workspace_add_surface (self->workspace, IDE_SURFACE (self->surface));
}

static void
gbp_sysprof_workspace_addin_load (IdeWorkspaceAddin *addin,
                                  IdeWorkspace      *workspace)
{
  GbpSysprofWorkspaceAddin *self = (GbpSysprofWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  self->workspace = workspace;

  sysprof_check_supported_async (NULL,
                                 gbp_sysprof_workspace_addin_check_supported_cb,
                                 g_object_ref (self));
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

  run_manager = ide_run_manager_from_context (context);
  ide_run_manager_remove_handler (run_manager, "profiler");

  if (self->surface != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->surface));

  self->surface = NULL;
  self->workspace = NULL;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_sysprof_workspace_addin_load;
  iface->unload = gbp_sysprof_workspace_addin_unload;
}
