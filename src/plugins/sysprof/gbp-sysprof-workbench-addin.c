/* gbp-sysprof-workbench-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-sysprof-workbench-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <sysprof-ui.h>

#include <libide-gui.h>
#include <libide-threading.h>

#include "gbp-sysprof-page.h"
#include "gbp-sysprof-workbench-addin.h"

struct _GbpSysprofWorkbenchAddin
{
  GObject       parent_instance;

  IdeWorkbench *workbench;

  guint         project_loaded : 1;
  guint         run_manager_busy : 1;
  guint         sanity_check : 1;
};

typedef struct
{
  GFile          *file;
  GbpSysprofPage *page;
} FindPageWithFile;

static void gbp_sysprof_workbench_addin_open_capture (GbpSysprofWorkbenchAddin *self,
                                                      GVariant                 *param);
static void gbp_sysprof_workbench_addin_run          (GbpSysprofWorkbenchAddin *self,
                                                      GVariant                 *param);

IDE_DEFINE_ACTION_GROUP (GbpSysprofWorkbenchAddin, gbp_sysprof_workbench_addin, {
  { "open-capture", gbp_sysprof_workbench_addin_open_capture },
  { "run", gbp_sysprof_workbench_addin_run },
})

static void
find_page_with_file (IdePage *page,
                     gpointer user_data)
{
  FindPageWithFile *find = user_data;
  GFile *file;

  g_assert (find != NULL);
  g_assert (!find->page || GBP_IS_SYSPROF_PAGE (find->page));
  g_assert (G_IS_FILE (find->file));

  if (find->page != NULL || !GBP_IS_SYSPROF_PAGE (page))
    return;

  if (!(file = gbp_sysprof_page_get_file (GBP_SYSPROF_PAGE (page))))
    return;

  if (g_file_equal (file, find->file))
    find->page = GBP_SYSPROF_PAGE (page);
}

static void
gbp_sysprof_workbench_addin_open_async (IdeWorkbenchAddin   *addin,
                                        GFile               *file,
                                        const char          *content_type,
                                        int                  at_line,
                                        int                  at_line_offset,
                                        IdeBufferOpenFlags   flags,
                                        IdePanelPosition    *position,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  IdeWorkspace *workspace;
  FindPageWithFile find = {file, NULL};

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (IDE_IS_WORKBENCH (self->workbench));


  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_sysprof_workbench_addin_open_async);

  ide_workbench_foreach_page (self->workbench, find_page_with_file, &find);

  if (find.page == NULL)
    {
      workspace = ide_workbench_get_current_workspace (self->workbench);
      find.page = gbp_sysprof_page_new_for_file (file);
      ide_workspace_add_page (workspace, IDE_PAGE (find.page), position);
    }

  workspace = ide_widget_get_workspace (GTK_WIDGET (find.page));
  panel_widget_raise (PANEL_WIDGET (find.page));
  gtk_window_present (GTK_WINDOW (workspace));

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static gboolean
gbp_sysprof_workbench_addin_open_finish (IdeWorkbenchAddin  *addin,
                                         GAsyncResult       *result,
                                         GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
gbp_sysprof_workbench_addin_can_open (IdeWorkbenchAddin *addin,
                                      GFile             *file,
                                      const char        *content_type,
                                      gint              *priority)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (addin));
  g_assert (G_IS_FILE (file));
  g_assert (priority != NULL);

  if (ide_str_equal0 (content_type, "application/x-sysprof-capture"))
    {
      *priority = 0;
      return TRUE;
    }

  return FALSE;
}

static void
gbp_sysprof_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
update_action_enabled (GbpSysprofWorkbenchAddin *self)
{
  gboolean enabled;

  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));

  enabled = !self->run_manager_busy && self->project_loaded && self->sanity_check;
  gbp_sysprof_workbench_addin_set_action_enabled (self, "run", enabled);
}

static void
gbp_sysprof_workbench_addin_notify_busy_cb (GbpSysprofWorkbenchAddin *self,
                                            GParamSpec               *pspec,
                                            IdeRunManager            *run_manager)
{
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  self->run_manager_busy = ide_run_manager_get_busy (run_manager);

  update_action_enabled (self);
}

static void
gbp_sysprof_workbench_addin_check_supported_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  g_autoptr(GbpSysprofWorkbenchAddin) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));

  if (!sysprof_check_supported_finish (result, &error))
    {
      g_warning ("Sysprof-3 is not supported, will not enable profiler: %s",
                 error->message);
      IDE_EXIT;
    }

  self->sanity_check = TRUE;

  update_action_enabled (self);

  IDE_EXIT;
}

static void
gbp_sysprof_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                            IdeProjectInfo    *project_info)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  self->project_loaded = TRUE;

  context = ide_workbench_get_context (self->workbench);
  run_manager = ide_run_manager_from_context (context);

  g_signal_connect_object (run_manager,
                           "notify::busy",
                           G_CALLBACK (gbp_sysprof_workbench_addin_notify_busy_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gbp_sysprof_workbench_addin_notify_busy_cb (self, NULL, run_manager);

  sysprof_check_supported_async (NULL,
                                 gbp_sysprof_workbench_addin_check_supported_cb,
                                 g_object_ref (self));
}

static void
gbp_sysprof_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_sysprof_workbench_addin_load;
  iface->unload = gbp_sysprof_workbench_addin_unload;
  iface->project_loaded = gbp_sysprof_workbench_addin_project_loaded;
  iface->can_open = gbp_sysprof_workbench_addin_can_open;
  iface->open_async = gbp_sysprof_workbench_addin_open_async;
  iface->open_finish = gbp_sysprof_workbench_addin_open_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSysprofWorkbenchAddin, gbp_sysprof_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_sysprof_workbench_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_sysprof_workbench_addin_class_init (GbpSysprofWorkbenchAddinClass *klass)
{
}

static void
gbp_sysprof_workbench_addin_init (GbpSysprofWorkbenchAddin *self)
{
  gbp_sysprof_workbench_addin_set_action_enabled (self, "run", FALSE);
}

static void
on_native_dialog_response_cb (GbpSysprofWorkbenchAddin *self,
                              int                       response_id,
                              GtkFileChooserNative     *native)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (GTK_IS_FILE_CHOOSER_NATIVE (native));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));
      g_autoptr(IdePanelPosition) position = ide_panel_position_new ();

      if (G_IS_FILE (file))
        gbp_sysprof_workbench_addin_open_async (IDE_WORKBENCH_ADDIN (self),
                                                file, NULL, 0, 0, 0,
                                                position, NULL, NULL, NULL);
    }

  gtk_native_dialog_hide (GTK_NATIVE_DIALOG (native));
  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
}

static void
gbp_sysprof_workbench_addin_open_capture (GbpSysprofWorkbenchAddin *self,
                                          GVariant                 *param)
{
  g_autoptr(GFile) workdir = NULL;
  GtkFileChooserNative *native;
  GtkFileFilter *filter;
  IdeWorkspace *workspace;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  context = ide_workbench_get_context (self->workbench);
  workdir = ide_context_ref_workdir (context);
  workspace = ide_workbench_get_current_workspace (self->workbench);

  native = gtk_file_chooser_native_new (_("Open Sysprof Capture…"),
                                        GTK_WINDOW (workspace),
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
                           G_CALLBACK (on_native_dialog_response_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
}

static void
gbp_sysprof_workbench_addin_run (GbpSysprofWorkbenchAddin *self,
                                 GVariant                 *param)
{
  PeasPluginInfo *plugin_info;
  IdeRunManager *run_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), "sysprof");
  context = ide_workbench_get_context (self->workbench);
  run_manager = ide_run_manager_from_context (context);

  ide_run_manager_set_run_tool_from_plugin_info (run_manager, plugin_info);
  ide_run_manager_run_async (run_manager, NULL, NULL, NULL);

  IDE_EXIT;
}
