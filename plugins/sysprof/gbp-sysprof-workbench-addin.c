/* gbp-sysprof-workbench-addin.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>
#include <sysprof.h>

#include "gbp-sysprof-perspective.h"
#include "gbp-sysprof-workbench-addin.h"

struct _GbpSysprofWorkbenchAddin
{
  GObject                parent_instance;

  GSimpleActionGroup    *actions;
  SpProfiler            *profiler;

  GbpSysprofPerspective *perspective;
  IdeWorkbench          *workbench;
};

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpSysprofWorkbenchAddin, gbp_sysprof_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
profiler_stopped (GbpSysprofWorkbenchAddin *self,
                  SpProfiler               *profiler)
{
  g_autoptr(SpCaptureReader) reader = NULL;
  g_autoptr(GError) error = NULL;
  SpCaptureWriter *writer;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_return_if_fail (SP_IS_PROFILER (profiler));

  if (self->profiler != profiler)
    IDE_EXIT;

  if (self->workbench == NULL)
    IDE_EXIT;

  writer = sp_profiler_get_writer (profiler);
  reader = sp_capture_writer_create_reader (writer, &error);

  if (reader == NULL)
    {
      /* TODO: Propagate error to an infobar or similar */
      g_warning ("%s", error->message);
      IDE_EXIT;
    }

  gbp_sysprof_perspective_set_reader (self->perspective, reader);

  ide_workbench_set_visible_perspective_name (self->workbench, "profiler");

  IDE_EXIT;
}

static void
profiler_child_spawned (GbpSysprofWorkbenchAddin *self,
                        const gchar              *identifier,
                        IdeRunner                *runner)
{
  GPid pid = 0;

  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
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

static void
profiler_run_handler (IdeRunManager *run_manager,
                      IdeRunner     *runner,
                      gpointer       user_data)
{
  GbpSysprofWorkbenchAddin *self = user_data;
  g_autoptr(SpSource) proc_source = NULL;
  g_autoptr(SpSource) perf_source = NULL;

  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  if (SP_IS_PROFILER (self->profiler))
    {
      if (sp_profiler_get_is_running (self->profiler))
        sp_profiler_stop (self->profiler);
      g_clear_object (&self->profiler);
    }

  self->profiler = sp_local_profiler_new ();

  sp_profiler_set_whole_system (SP_PROFILER (self->profiler), FALSE);

  proc_source = sp_proc_source_new ();
  sp_profiler_add_source (self->profiler, proc_source);

  perf_source = sp_perf_source_new ();
  sp_profiler_add_source (self->profiler, perf_source);

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
}

static void
gbp_sysprof_workbench_addin_open_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)object;
  g_autoptr(SpCaptureReader) reader = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (G_IS_TASK (result));

  reader = g_task_propagate_pointer (G_TASK (result), &error);

  g_assert (reader || error != NULL);

  if (reader == NULL)
    {
      g_message ("%s", error->message);
      return;
    }

  gbp_sysprof_perspective_set_reader (self->perspective, reader);
}

static void
gbp_sysprof_workbench_addin_open_worker (GTask        *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  GbpSysprofWorkbenchAddin *self = source_object;
  g_autofree gchar *path = NULL;
  SpCaptureReader *reader;
  GFile *file = task_data;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  path = g_file_get_path (file);

  if (NULL == (reader = sp_capture_reader_new (path, &error)))
    {
      g_assert (error != NULL);
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, reader, (GDestroyNotify)sp_capture_reader_unref);
}

static void
gbp_sysprof_workbench_addin_open (GbpSysprofWorkbenchAddin *self,
                                  GFile                    *file)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));

  if (!g_file_is_native (file))
    {
      g_warning ("Can only open local sysprof capture files.");
      return;
    }

  task = g_task_new (self, NULL, gbp_sysprof_workbench_addin_open_cb, NULL);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);
  g_task_run_in_thread (task, gbp_sysprof_workbench_addin_open_worker);
}

static void
open_profile_action (GSimpleAction *action,
                     GVariant      *variant,
                     gpointer       user_data)
{
  GbpSysprofWorkbenchAddin *self = user_data;
  GtkFileChooserNative *native;
  GtkFileFilter *filter;
  gint ret;

  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));
  g_assert (GBP_IS_SYSPROF_PERSPECTIVE (self->perspective));

  ide_workbench_set_visible_perspective (self->workbench, IDE_PERSPECTIVE (self->perspective));

  native = gtk_file_chooser_native_new (_("Open Profile"),
                                        GTK_WINDOW (self->workbench),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Open"),
                                        _("Cancel"));

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
        gbp_sysprof_workbench_addin_open (self, file);
    }

  gtk_native_dialog_hide (GTK_NATIVE_DIALOG (native));
  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
}

static void
gbp_sysprof_workbench_addin_finalize (GObject *object)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)object;

  g_clear_object (&self->actions);

  G_OBJECT_CLASS (gbp_sysprof_workbench_addin_parent_class)->finalize (object);
}

static void
gbp_sysprof_workbench_addin_class_init (GbpSysprofWorkbenchAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_sysprof_workbench_addin_finalize;
}

static void
gbp_sysprof_workbench_addin_init (GbpSysprofWorkbenchAddin *self)
{
  static const GActionEntry entries[] = {
    { "open-profile", open_profile_action },
  };

  self->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
}

static void
run_manager_stopped (GbpSysprofWorkbenchAddin *self,
                     IdeRunManager            *run_manager)
{
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  if (self->profiler != NULL && sp_profiler_get_is_running (self->profiler))
    sp_profiler_stop (self->profiler);
}

static void
gbp_sysprof_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  context = ide_workbench_get_context (workbench);

  /*
   * Register our custom run handler to activate the profiler.
   */
  run_manager = ide_context_get_run_manager (context);
  ide_run_manager_add_handler (run_manager,
                               "profiler",
                               _("Run with Profiler"),
                               "utilities-system-monitor-symbolic",
                               "<Control>F8",
                               profiler_run_handler,
                               self,
                               NULL);
  g_signal_connect_object (run_manager,
                           "stopped",
                           G_CALLBACK (run_manager_stopped),
                           self,
                           G_CONNECT_SWAPPED);

  /*
   * Add the perspcetive to the workbench.
   */
  self->perspective = g_object_new (GBP_TYPE_SYSPROF_PERSPECTIVE,
                                    "visible", TRUE,
                                    NULL);
  ide_workbench_add_perspective (workbench, IDE_PERSPECTIVE (self->perspective));


  /*
   * Add our actions to the workbench so they can be activated via the
   * headerbar or the perspective.
   */
  gtk_widget_insert_action_group (GTK_WIDGET (workbench),
                                  "profiler",
                                  G_ACTION_GROUP (self->actions));
}

static void
gbp_sysprof_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  context = ide_workbench_get_context (workbench);

  run_manager = ide_context_get_run_manager (context);
  ide_run_manager_remove_handler (run_manager, "profiler");

  ide_workbench_remove_perspective (workbench, IDE_PERSPECTIVE (self->perspective));

  self->perspective = NULL;
  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_sysprof_workbench_addin_load;
  iface->unload = gbp_sysprof_workbench_addin_unload;
}
