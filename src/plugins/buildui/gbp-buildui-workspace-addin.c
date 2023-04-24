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
#include "gbp-buildui-status-indicator.h"
#include "gbp-buildui-status-popover.h"
#include "gbp-buildui-targets-dialog.h"
#include "gbp-buildui-workspace-addin.h"

struct _GbpBuilduiWorkspaceAddin
{
  GObject                    parent_instance;

  /* Borrowed references */
  IdeWorkspace              *workspace;
  GbpBuilduiOmniBarSection  *omni_bar_section;
  GbpBuilduiLogPane         *log_pane;
  GbpBuilduiPane            *pane;
  GtkBox                    *diag_box;
  GtkImage                  *error_image;
  GtkLabel                  *error_label;
  GtkImage                  *warning_image;
  GtkLabel                  *warning_label;
  GtkMenuButton             *status_button;

  /* Owned references */
  GSignalGroup             *build_manager_signals;
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
                "action-name", busy ? "context.build-manager.cancel" : "context.build-manager.build",
                "action-tooltip", busy ? _("Stop Building Project (Shift+Ctrl+Alt+C)") : _("Build Project (Shift+Ctrl+Alt+B)"),
                NULL);
}

static void
gbp_buildui_workspace_addin_bind_build_manager (GbpBuilduiWorkspaceAddin *self,
                                                IdeBuildManager          *build_manager,
                                                GSignalGroup             *signals)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (G_IS_SIGNAL_GROUP (signals));

  gbp_buildui_workspace_addin_notify_busy (self, NULL, build_manager);
  gbp_buildui_workspace_addin_notify_pipeline (self, NULL, build_manager);
  gbp_buildui_workspace_addin_notify_error_count (self, NULL, build_manager);
  gbp_buildui_workspace_addin_notify_warning_count (self, NULL, build_manager);
}

static void
on_view_output_cb (GbpBuilduiWorkspaceAddin *self,
                   GVariant                 *param)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));

  panel_widget_raise (PANEL_WIDGET (self->log_pane));
  gtk_widget_grab_focus (GTK_WIDGET (self->log_pane));
}

static void
select_build_target_action (GbpBuilduiWorkspaceAddin *self,
                            GVariant                 *param)
{
  GbpBuilduiTargetsDialog *dialog;
  IdeContext *context;

  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));

  context = ide_workspace_get_context (self->workspace);
  dialog = g_object_new (GBP_TYPE_BUILDUI_TARGETS_DIALOG,
                         "context", context,
                         "transient-for", self->workspace,
                         "modal", TRUE,
                         NULL);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
show_status_popover (GbpBuilduiWorkspaceAddin *self,
                     GVariant                 *param)
{
  GtkPopover *popover;

  IDE_ENTRY;

  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  popover = gtk_menu_button_get_popover (self->status_button);
  gbp_buildui_status_popover_set_page (GBP_BUILDUI_STATUS_POPOVER (popover),
                                       g_variant_get_string (param, NULL));
  gtk_menu_button_popup (self->status_button);

  IDE_EXIT;
}

IDE_DEFINE_ACTION_GROUP (GbpBuilduiWorkspaceAddin, gbp_buildui_workspace_addin, {
  { "build-target.select", select_build_target_action },
  { "log.show", on_view_output_cb },
  { "status.show", show_status_popover, "s" },
})

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

  if (phase > IDE_PIPELINE_PHASE_CONFIGURE &&
      g_settings_get_boolean (settings, "show-log-for-build"))
    panel_widget_raise (PANEL_WIDGET (self->log_pane));

  IDE_EXIT;
}

static void
gbp_buildui_workspace_addin_load (IdeWorkspaceAddin *addin,
                                  IdeWorkspace      *workspace)
{
  GbpBuilduiWorkspaceAddin *self = (GbpBuilduiWorkspaceAddin *)addin;
  g_autoptr(PanelPosition) pane_position = NULL;
  g_autoptr(PanelPosition) log_position = NULL;
  PangoAttrList *small_attrs = NULL;
  IdeBuildManager *build_manager;
  PanelStatusbar *statusbar;
  IdeWorkbench *workbench;
  IdeOmniBar *omnibar;
  IdeContext *context;

  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  self->workspace = workspace;

  omnibar = ide_primary_workspace_get_omni_bar (IDE_PRIMARY_WORKSPACE (workspace));
  workbench = ide_widget_get_workbench (GTK_WIDGET (workspace));
  context = ide_workbench_get_context (workbench);
  build_manager = ide_build_manager_from_context (context);

  statusbar = ide_workspace_get_statusbar (workspace);
  self->status_button = g_object_new (GTK_TYPE_MENU_BUTTON,
                                      "child", gbp_buildui_status_indicator_new (context),
                                      "popover", gbp_buildui_status_popover_new (context),
                                      "direction", GTK_ARROW_UP,
                                      "focus-on-click", FALSE,
                                      "tooltip-text", _("Display Build Diagnostics (Ctrl+Alt+?)"),
                                      NULL);
  panel_statusbar_add_prefix (statusbar, 1000, GTK_WIDGET (self->status_button));

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

  log_position = panel_position_new ();
  panel_position_set_area (log_position, PANEL_AREA_BOTTOM);
  panel_position_set_depth (log_position, 2);

  self->log_pane = g_object_new (GBP_TYPE_BUILDUI_LOG_PANE, NULL);
  ide_workspace_add_pane (workspace, IDE_PANE (self->log_pane), log_position);

  pane_position = panel_position_new ();
  panel_position_set_area (pane_position, PANEL_AREA_START);
  panel_position_set_depth (pane_position, 1);

  self->pane = g_object_new (GBP_TYPE_BUILDUI_PANE, NULL);
  ide_workspace_add_pane (workspace, IDE_PANE (self->pane), pane_position);

  self->build_manager_signals = g_signal_group_new (IDE_TYPE_BUILD_MANAGER);
  g_signal_connect_object (self->build_manager_signals,
                           "bind",
                           G_CALLBACK (gbp_buildui_workspace_addin_bind_build_manager),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "notify::error-count",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_error_count),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "notify::warning-count",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_warning_count),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "notify::pipeline",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_pipeline),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "notify::busy",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_busy),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->build_manager_signals,
                                   "build-started",
                                   G_CALLBACK (gbp_buildui_workspace_addin_build_started),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_set_target (self->build_manager_signals, build_manager);
}

static void
gbp_buildui_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                    IdeWorkspace      *workspace)
{
  GbpBuilduiWorkspaceAddin *self = (GbpBuilduiWorkspaceAddin *)addin;
  PanelStatusbar *statusbar;

  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  statusbar = ide_workspace_get_statusbar (workspace);
  panel_statusbar_remove (statusbar, GTK_WIDGET (self->status_button));
  self->status_button = NULL;

  if (self->omni_bar_section)
    gtk_widget_unparent (GTK_WIDGET (self->omni_bar_section));

  if (self->diag_box)
    gtk_widget_unparent (GTK_WIDGET (self->diag_box));

  g_signal_group_set_target (self->build_manager_signals, NULL);
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
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_buildui_workspace_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_buildui_workspace_addin_class_init (GbpBuilduiWorkspaceAddinClass *klass)
{
}

static void
gbp_buildui_workspace_addin_init (GbpBuilduiWorkspaceAddin *self)
{
}
