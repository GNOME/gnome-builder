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
  PeasPluginInfo *plugin_info;

  g_assert (GBP_IS_SYSPROF_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (IDE_IS_RUN_MANAGER (self->run_manager));

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), "sysprof");
  ide_run_manager_set_run_tool_from_plugin_info (self->run_manager, plugin_info);
  ide_run_manager_run_async (self->run_manager, NULL, NULL, NULL);
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

  IDE_EXIT;
}

static const GActionEntry entries[] = {
  { "open-capture", open_capture_action },
  { "run", run_cb },
};

static void
gbp_sysprof_workspace_addin_load (IdeWorkspaceAddin *addin,
                                  IdeWorkspace      *workspace)
{
  GbpSysprofWorkspaceAddin *self = (GbpSysprofWorkspaceAddin *)addin;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_auto(GStrv) keys = NULL;
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

  settings = g_settings_new ("org.gnome.builder.sysprof");
  g_object_get (settings, "settings-schema", &schema, NULL);
  keys = g_settings_schema_list_keys (schema);

  for (guint i = 0; keys[i]; i++)
    {
      g_autoptr(GAction) action = g_settings_create_action (settings, keys[i]);
      g_action_map_add_action (G_ACTION_MAP (self->actions), action);
    }

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

