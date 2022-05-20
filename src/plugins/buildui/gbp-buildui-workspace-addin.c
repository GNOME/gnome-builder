/* gbp-buildui-workspace-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-buildui-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-editor.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-buildui-log-pane.h"
#include "gbp-buildui-omni-bar-section.h"
#include "gbp-buildui-pane.h"
#include "gbp-buildui-targets-dialog.h"
#include "gbp-buildui-workspace-addin.h"

struct _GbpBuilduiWorkspaceAddin
{
  GObject                   parent_instance;

  /* Borrowed references */
  IdeWorkspace             *workspace;
  GbpBuilduiOmniBarSection *omni_bar_section;
  GbpBuilduiLogPane        *log_pane;
  GbpBuilduiPane           *pane;
  GtkBox                   *diag_box;
  GtkImage                 *error_image;
  GtkLabel                 *error_label;
  GtkImage                 *warning_image;
  GtkLabel                 *warning_label;

  /* Owned references */
  IdeSignalGroup           *build_manager_signals;
};

static void
gbp_buildui_workspace_addin_notify_error_count (GbpBuilduiWorkspaceAddin *self,
                                                GParamSpec               *pspec,
                                                IdeBuildManager          *build_manager)
{
  gchar str[12];
  guint count;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  if (!(count = ide_build_manager_get_error_count (build_manager)))
    {
      gtk_widget_set_visible (GTK_WIDGET (self->error_label), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->error_image), FALSE);
      gtk_label_set_label (self->error_label, NULL);
      return;
    }

  g_snprintf (str, sizeof str, "%u", count);
  gtk_label_set_label (self->error_label, str);
  gtk_widget_set_visible (GTK_WIDGET (self->error_label), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->error_image), TRUE);

  if (count > 0)
    panel_widget_set_needs_attention (PANEL_WIDGET (self->pane), TRUE);
}

static void
gbp_buildui_workspace_addin_notify_warning_count (GbpBuilduiWorkspaceAddin *self,
                                                  GParamSpec               *pspec,
                                                  IdeBuildManager          *build_manager)
{
  gchar str[12];
  guint count;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  if (!(count = ide_build_manager_get_warning_count (build_manager)))
    {
      gtk_widget_set_visible (GTK_WIDGET (self->warning_label), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->warning_image), FALSE);
      gtk_label_set_label (self->warning_label, NULL);
      return;
    }

  g_snprintf (str, sizeof str, "%u", count);
  gtk_label_set_label (self->warning_label, str);
  gtk_widget_set_visible (GTK_WIDGET (self->warning_label), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->warning_image), TRUE);

  if (count > 0)
    panel_widget_set_needs_attention (PANEL_WIDGET (self->pane), TRUE);
}

static void
gbp_buildui_workspace_addin_notify_pipeline (GbpBuilduiWorkspaceAddin *self,
                                             GParamSpec               *pspec,
                                             IdeBuildManager          *build_manager)
{
  IdePipeline *pipeline;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  pipeline = ide_build_manager_get_pipeline (build_manager);
  gbp_buildui_log_pane_set_pipeline (self->log_pane, pipeline);
  gbp_buildui_pane_set_pipeline (self->pane, pipeline);
}

static void
gbp_buildui_workspace_addin_notify_busy (GbpBuilduiWorkspaceAddin *self,
                                         GParamSpec               *pspec,
                                         IdeBuildManager          *build_manager)
{
  IdeOmniBar *omni_bar;
  gboolean busy;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (self->workspace));

  omni_bar = ide_primary_workspace_get_omni_bar (IDE_PRIMARY_WORKSPACE (self->workspace));
  busy = ide_build_manager_get_busy (build_manager);

  g_object_set (omni_bar,
                "icon-name", busy ? "builder-build-stop-symbolic" : "builder-build-symbolic",
                "action-name", busy ? "build-manager.cancel" : "build-manager.build",
                NULL);
}

static void
gbp_buildui_workspace_addin_bind_build_manager (GbpBuilduiWorkspaceAddin *self,
                                                IdeBuildManager          *build_manager,
                                                IdeSignalGroup           *signals)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (IDE_IS_SIGNAL_GROUP (signals));

  gbp_buildui_workspace_addin_notify_busy (self, NULL, build_manager);
  gbp_buildui_workspace_addin_notify_pipeline (self, NULL, build_manager);
  gbp_buildui_workspace_addin_notify_error_count (self, NULL, build_manager);
  gbp_buildui_workspace_addin_notify_warning_count (self, NULL, build_manager);
}

static void
on_view_output_cb (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  GbpBuilduiWorkspaceAddin *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));

  panel_widget_raise (PANEL_WIDGET (self->log_pane));
  gtk_widget_grab_focus (GTK_WIDGET (self->log_pane));
}

static void
select_build_target_action (GSimpleAction *action,
                            GVariant      *param,
                            gpointer       user_data)
{
  GbpBuilduiWorkspaceAddin *self = user_data;
  GbpBuilduiTargetsDialog *dialog;
  IdeContext *context;

  g_assert (G_IS_SIMPLE_ACTION (action));

  context = ide_workspace_get_context (self->workspace);
  dialog = g_object_new (GBP_TYPE_BUILDUI_TARGETS_DIALOG,
                         "context", context,
                         "transient-for", self->workspace,
                         "modal", TRUE,
                         NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}

static const GActionEntry actions[] = {
  { "show-build-log", on_view_output_cb },
  { "select-build-target", select_build_target_action },
};

static void
gbp_buildui_workspace_addin_build_started (GbpBuilduiWorkspaceAddin *self,
                                           IdePipeline              *pipeline,
                                           IdeBuildManager          *build_manager)
{
  IdePipelinePhase phase;
  g_autoptr(GSettings) settings = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  phase = ide_pipeline_get_requested_phase (pipeline);

  IDE_TRACE_MSG ("Pipeline phase 0x%x requested", phase);

  settings = g_settings_new ("org.gnome.builder.build");
  if (g_settings_get_boolean (settings, "clear-build-log-pane"))
      gbp_buildui_log_pane_clear (self->log_pane);

  if (phase > IDE_PIPELINE_PHASE_CONFIGURE)
    panel_widget_raise (PANEL_WIDGET (self->log_pane));

  IDE_EXIT;
}

static void
gbp_buildui_workspace_addin_load (IdeWorkspaceAddin *addin,
                                  IdeWorkspace      *workspace)
{
  GbpBuilduiWorkspaceAddin *self = (GbpBuilduiWorkspaceAddin *)addin;
  g_autoptr(IdePanelPosition) pane_position = NULL;
  g_autoptr(IdePanelPosition) log_position = NULL;
  PangoAttrList *small_attrs = NULL;
  IdeBuildManager *build_manager;
  IdeWorkbench *workbench;
  IdeOmniBar *omnibar;
  IdeContext *context;

  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  self->workspace = workspace;

  g_action_map_add_action_entries (G_ACTION_MAP (workspace),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  omnibar = ide_primary_workspace_get_omni_bar (IDE_PRIMARY_WORKSPACE (workspace));
  workbench = ide_widget_get_workbench (GTK_WIDGET (workspace));
  context = ide_workbench_get_context (workbench);
  build_manager = ide_build_manager_from_context (context);

  small_attrs = pango_attr_list_new ();
  pango_attr_list_insert (small_attrs, pango_attr_scale_new (0.833333));

  self->diag_box = g_object_new (GTK_TYPE_BOX,
                                 "orientation", GTK_ORIENTATION_HORIZONTAL,
                                 NULL);
  g_signal_connect (self->diag_box,
                    "destroy",
                    G_CALLBACK (ide_gtk_widget_destroyed),
                    &self->diag_box);
  ide_omni_bar_add_status_icon (omnibar, GTK_WIDGET (self->diag_box), 0);

  self->error_image = g_object_new (GTK_TYPE_IMAGE,
                                    "icon-name", "dialog-error-symbolic",
                                    "margin-end", 2,
                                    "margin-start", 4,
                                    "pixel-size", 12,
                                    "valign", GTK_ALIGN_BASELINE,
                                    "visible", FALSE,
                                    NULL);
  gtk_box_append (self->diag_box, GTK_WIDGET (self->error_image));

  self->error_label = g_object_new (GTK_TYPE_LABEL,
                                    "attributes", small_attrs,
                                    "margin-end", 2,
                                    "margin-start", 2,
                                    "valign", GTK_ALIGN_BASELINE,
                                    "visible", FALSE,
                                    NULL);
  gtk_box_append (self->diag_box, GTK_WIDGET (self->error_label));

  self->warning_image = g_object_new (GTK_TYPE_IMAGE,
                                      "icon-name", "dialog-warning-symbolic",
                                      "margin-end", 2,
                                      "margin-start", 4,
                                      "pixel-size", 12,
                                      "valign", GTK_ALIGN_BASELINE,
                                      "visible", FALSE,
                                      NULL);
  gtk_box_append (self->diag_box, GTK_WIDGET (self->warning_image));

  self->warning_label = g_object_new (GTK_TYPE_LABEL,
                                      "attributes", small_attrs,
                                      "margin-end", 2,
                                      "margin-start", 2,
                                      "valign", GTK_ALIGN_BASELINE,
                                      "visible", FALSE,
                                      NULL);
  gtk_box_append (self->diag_box, GTK_WIDGET (self->warning_label));

  g_clear_pointer (&small_attrs, pango_attr_list_unref);

  self->omni_bar_section = g_object_new (GBP_TYPE_BUILDUI_OMNI_BAR_SECTION, NULL);
  g_signal_connect (self->omni_bar_section,
                    "destroy",
                    G_CALLBACK (ide_gtk_widget_destroyed),
                    &self->omni_bar_section);
  ide_omni_bar_add_popover_section (omnibar, GTK_WIDGET (self->omni_bar_section), 0);
  gbp_buildui_omni_bar_section_set_context (self->omni_bar_section, context);

  log_position = ide_panel_position_new ();
  ide_panel_position_set_edge (log_position, PANEL_DOCK_POSITION_BOTTOM);
  ide_panel_position_set_depth (log_position, 2);

  self->log_pane = g_object_new (GBP_TYPE_BUILDUI_LOG_PANE, NULL);
  ide_workspace_add_pane (workspace, IDE_PANE (self->log_pane), log_position);

  pane_position = ide_panel_position_new ();
  ide_panel_position_set_edge (pane_position, PANEL_DOCK_POSITION_START);
  ide_panel_position_set_depth (pane_position, 1);

  self->pane = g_object_new (GBP_TYPE_BUILDUI_PANE, NULL);
  ide_workspace_add_pane (workspace, IDE_PANE (self->pane), pane_position);

  self->build_manager_signals = ide_signal_group_new (IDE_TYPE_BUILD_MANAGER);
  g_signal_connect_object (self->build_manager_signals,
                           "bind",
                           G_CALLBACK (gbp_buildui_workspace_addin_bind_build_manager),
                           self,
                           G_CONNECT_SWAPPED);
  ide_signal_group_connect_object (self->build_manager_signals,
                                   "notify::error-count",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_error_count),
                                   self,
                                   G_CONNECT_SWAPPED);
  ide_signal_group_connect_object (self->build_manager_signals,
                                   "notify::warning-count",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_warning_count),
                                   self,
                                   G_CONNECT_SWAPPED);
  ide_signal_group_connect_object (self->build_manager_signals,
                                   "notify::pipeline",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_pipeline),
                                   self,
                                   G_CONNECT_SWAPPED);
  ide_signal_group_connect_object (self->build_manager_signals,
                                   "notify::busy",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_busy),
                                   self,
                                   G_CONNECT_SWAPPED);
  ide_signal_group_connect_object (self->build_manager_signals,
                                   "build-started",
                                   G_CALLBACK (gbp_buildui_workspace_addin_build_started),
                                   self,
                                   G_CONNECT_SWAPPED);
  ide_signal_group_set_target (self->build_manager_signals, build_manager);
}

static void
gbp_buildui_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                    IdeWorkspace      *workspace)
{
  GbpBuilduiWorkspaceAddin *self = (GbpBuilduiWorkspaceAddin *)addin;

  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  for (guint i = 0; i < G_N_ELEMENTS (actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (workspace), actions[i].name);

  if (self->omni_bar_section)
    gtk_widget_unparent (GTK_WIDGET (self->omni_bar_section));

  if (self->diag_box)
    gtk_widget_unparent (GTK_WIDGET (self->diag_box));

  ide_signal_group_set_target (self->build_manager_signals, NULL);
  g_clear_object (&self->build_manager_signals);

  self->workspace = NULL;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_buildui_workspace_addin_load;
  iface->unload = gbp_buildui_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpBuilduiWorkspaceAddin, gbp_buildui_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_buildui_workspace_addin_class_init (GbpBuilduiWorkspaceAddinClass *klass)
{
}

static void
gbp_buildui_workspace_addin_init (GbpBuilduiWorkspaceAddin *self)
{
}
