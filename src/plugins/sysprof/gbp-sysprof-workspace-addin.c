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

#include <glib/gi18n.h>

#include <sysprof-ui.h>

#include "gbp-sysprof-page.h"
#include "gbp-sysprof-workspace-addin.h"

struct _GbpSysprofWorkspaceAddin
{
  GObject             parent_instance;

  IdeWorkspace       *workspace;

  GSimpleActionGroup *actions;
  IdeRunManager      *run_manager;
};

static void
set_state (GSimpleAction *action,
           GVariant      *param,
           gpointer       user_data)
{
  g_simple_action_set_state (action, param);
}

static gboolean
get_state (GbpSysprofWorkspaceAddin *self,
           const char               *action_name)
{
  g_autoptr(GVariant) state = NULL;
  GAction *action;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (action_name != NULL);

  if (!(action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), action_name)))
    g_return_val_if_reached (FALSE);

  if (!(state = g_action_get_state (action)))
    g_return_val_if_reached (FALSE);

  if (!g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    g_return_val_if_reached (FALSE);

  return g_variant_get_boolean (state);
}

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
  g_autoptr(SysprofSpawnable) spawnable = NULL;
  g_autoptr(IdePanelPosition) position = NULL;
  g_autoptr(GPtrArray) sources = NULL;
  g_auto(GStrv) argv = NULL;
  const gchar * const *env;
  GbpSysprofPage *page;
  IdeEnvironment *ienv;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  sources = g_ptr_array_new_with_free_func (g_object_unref);

  profiler = sysprof_local_profiler_new ();

  /*
   * Currently we require whole-system because otherwise we can get a situation
   * where we only watch the spawning process (say jhbuild, flatpak, etc).
   * Longer term we either need a way to follow-children and/or limit to a
   * cgroup/process-group.
   */
  sysprof_profiler_set_whole_system (profiler, TRUE);

#ifdef __linux__
  {
    g_ptr_array_add (sources, sysprof_proc_source_new ());

    if (get_state (self, "cpu-aid"))
      g_ptr_array_add (sources, sysprof_hostinfo_source_new ());

    if (get_state (self, "perf-aid"))
      g_ptr_array_add (sources, sysprof_perf_source_new ());

    if (get_state (self, "memory-aid"))
      g_ptr_array_add (sources, sysprof_memory_source_new ());

    if (get_state (self, "energy-aid"))
      g_ptr_array_add (sources, sysprof_proxy_source_new (G_BUS_TYPE_SYSTEM,
                                                          "org.gnome.Sysprof3",
                                                          "/org/gnome/Sysprof3/RAPL"));

    if (get_state (self, "battery-aid"))
      g_ptr_array_add (sources, sysprof_battery_source_new ());

    if (get_state (self, "netstat-aid"))
      g_ptr_array_add (sources, sysprof_netdev_source_new ());

    if (get_state (self, "diskstat-aid"))
      g_ptr_array_add (sources, sysprof_diskstat_source_new ());

    if (!get_state (self, "allow-throttle"))
      {
        SysprofSource *governor = sysprof_governor_source_new ();
        sysprof_governor_source_set_disable_governor (SYSPROF_GOVERNOR_SOURCE (governor), TRUE);
        g_ptr_array_add (sources, governor);
      }
  }
#endif

  if (get_state (self, "memprof-aid"))
    g_ptr_array_add (sources, sysprof_memprof_source_new ());

  g_ptr_array_add (sources, sysprof_gjs_source_new ());
  g_ptr_array_add (sources, sysprof_symbols_source_new ());

  /* Allow the app to submit us data if it supports "SYSPROF_TRACE_FD" */
  if (get_state (self, "allow-tracefd"))
    {
      SysprofSource *app_source = sysprof_tracefd_source_new ();
      sysprof_tracefd_source_set_envvar (SYSPROF_TRACEFD_SOURCE (app_source), "SYSPROF_TRACE_FD");
      g_ptr_array_add (sources, app_source);
    }

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

      if (source != NULL)
        {
          sysprof_profiler_add_source (profiler, source);
          sysprof_source_modify_spawn (source, spawnable);
        }
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

  page = gbp_sysprof_page_new_for_profiler (profiler);
  position = ide_panel_position_new ();
  ide_workspace_add_page (self->workspace, IDE_PAGE (page), position);
}

static void
gbp_sysprof_workspace_addin_open (GbpSysprofWorkspaceAddin *self,
                                  GFile                    *file)
{
  g_autoptr(IdePanelPosition) position = NULL;
  GbpSysprofPage *page;

  IDE_ENTRY;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (G_IS_FILE (file));

  if (self->workspace == NULL)
    IDE_EXIT;

  if (!g_file_is_native (file))
    {
      g_warning ("Can only open local sysprof capture files.");
      return;
    }

  position = ide_panel_position_new ();
  page = gbp_sysprof_page_new_for_file (file);

  ide_workspace_add_page (self->workspace, IDE_PAGE (page), position);

  IDE_EXIT;
}

static void
on_native_dialog_respnose_cb (GbpSysprofWorkspaceAddin *self,
                              int                       response_id,
                              GtkFileChooserNative     *native)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (GTK_IS_FILE_CHOOSER_NATIVE (native));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));

      if (G_IS_FILE (file))
        gbp_sysprof_workspace_addin_open (self, file);
    }

  gtk_native_dialog_hide (GTK_NATIVE_DIALOG (native));
  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
}

static void
open_capture_action (GSimpleAction *action,
                     GVariant      *variant,
                     gpointer       user_data)
{
  GbpSysprofWorkspaceAddin *self = user_data;
  g_autoptr(GFile) workdir = NULL;
  GtkFileChooserNative *native;
  GtkFileFilter *filter;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  context = ide_workspace_get_context (self->workspace);
  workdir = ide_context_ref_workdir (context);

  native = gtk_file_chooser_native_new (_("Open Sysprof Capture…"),
                                        GTK_WINDOW (self->workspace),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Open"),
                                        _("Cancel"));
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (native), workdir, NULL);

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

  g_signal_connect_object (native,
                           "response",
                           G_CALLBACK (on_native_dialog_respnose_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
}

static void
run_cb (GSimpleAction *action,
        GVariant      *param,
        gpointer       user_data)
{
  GbpSysprofWorkspaceAddin *self = user_data;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (IDE_IS_RUN_MANAGER (self->run_manager));

  ide_run_manager_set_handler (self->run_manager, "sysprof");
  ide_run_manager_run_async (self->run_manager, NULL, NULL, NULL, NULL);
}

static void
gbp_sysprof_workspace_addin_check_supported_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  g_autoptr(GbpSysprofWorkspaceAddin) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));

  if (!sysprof_check_supported_finish (result, &error))
    {
      g_warning ("Sysprof-3 is not supported, will not enable profiler: %s",
                 error->message);
      IDE_EXIT;
    }

  if (self->workspace == NULL)
    IDE_EXIT;

  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (IDE_IS_RUN_MANAGER (self->run_manager));

  gtk_widget_insert_action_group (GTK_WIDGET (self->workspace),
                                  "sysprof",
                                  G_ACTION_GROUP (self->actions));
  ide_run_manager_add_handler (self->run_manager,
                               "sysprof",
                               _("Run with Profiler"),
                               "builder-profiler-symbolic",
                               "<Control>F8",
                               profiler_run_handler,
                               self,
                               NULL);

  IDE_EXIT;
}

static const GActionEntry entries[] = {
  { "open-capture", open_capture_action },
  { "run", run_cb },
  { "cpu-aid", NULL, NULL, "true", set_state },
  { "perf-aid", NULL, NULL, "true", set_state },
  { "memory-aid", NULL, NULL, "true", set_state },
  { "memprof-aid", NULL, NULL, "false", set_state },
  { "diskstat-aid", NULL, NULL, "true", set_state },
  { "netstat-aid", NULL, NULL, "true", set_state },
  { "energy-aid", NULL, NULL, "false", set_state },
  { "battery-aid", NULL, NULL, "false", set_state },
  { "compositor-aid", NULL, NULL, "false", set_state },
  { "allow-throttle", NULL, NULL, "true", set_state },
  { "allow-tracefd", NULL, NULL, "true", set_state },
};

static void
gbp_sysprof_workspace_addin_load (IdeWorkspaceAddin *addin,
                                  IdeWorkspace      *workspace)
{
  GbpSysprofWorkspaceAddin *self = (GbpSysprofWorkspaceAddin *)addin;
  IdeRunManager *run_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  self->workspace = workspace;

  context = ide_workspace_get_context (workspace);
  run_manager = ide_run_manager_from_context (context);

  self->run_manager = g_object_ref (run_manager);
  self->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);

  g_object_bind_property (self->run_manager,
                          "busy",
                          g_action_map_lookup_action (G_ACTION_MAP (self->actions), "run"),
                          "enabled",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  sysprof_check_supported_async (NULL,
                                 gbp_sysprof_workspace_addin_check_supported_cb,
                                 g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_sysprof_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                    IdeWorkspace      *workspace)
{
  GbpSysprofWorkspaceAddin *self = (GbpSysprofWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "sysprof", NULL);
  ide_run_manager_remove_handler (self->run_manager, "sysprof");

  g_clear_object (&self->actions);
  g_clear_object (&self->run_manager);

  self->workspace = NULL;

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_sysprof_workspace_addin_load;
  iface->unload = gbp_sysprof_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSysprofWorkspaceAddin, gbp_sysprof_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_sysprof_workspace_addin_class_init (GbpSysprofWorkspaceAddinClass *klass)
{
}

static void
gbp_sysprof_workspace_addin_init (GbpSysprofWorkspaceAddin *self)
{
}

