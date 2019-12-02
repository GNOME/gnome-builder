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

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libide-editor.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-buildui-config-surface.h"
#include "gbp-buildui-log-pane.h"
#include "gbp-buildui-omni-bar-section.h"
#include "gbp-buildui-pane.h"
#include "gbp-buildui-workspace-addin.h"

struct _GbpBuilduiWorkspaceAddin
{
  GObject                   parent_instance;

  /* Borrowed references */
  IdeWorkspace             *workspace;
  GbpBuilduiConfigSurface  *surface;
  GbpBuilduiOmniBarSection *omni_bar_section;
  GbpBuilduiLogPane        *log_pane;
  GbpBuilduiPane           *pane;
  GtkBox                   *diag_box;
  GtkImage                 *error_image;
  GtkLabel                 *error_label;
  GtkImage                 *warning_image;
  GtkLabel                 *warning_label;
  GtkButton                *build_button;
  GtkButton                *cancel_button;

  /* Owned references */
  DzlSignalGroup           *build_manager_signals;
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
    dzl_dock_item_needs_attention (DZL_DOCK_ITEM (self->pane));
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
    dzl_dock_item_needs_attention (DZL_DOCK_ITEM (self->pane));
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
  gboolean busy;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  busy = ide_build_manager_get_busy (build_manager);

  gtk_widget_set_visible (GTK_WIDGET (self->build_button), !busy);
  gtk_widget_set_visible (GTK_WIDGET (self->cancel_button), busy);
}

static void
gbp_buildui_workspace_addin_bind_build_manager (GbpBuilduiWorkspaceAddin *self,
                                                IdeBuildManager          *build_manager,
                                                DzlSignalGroup           *signals)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (DZL_IS_SIGNAL_GROUP (signals));

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

  ide_widget_reveal_and_grab (GTK_WIDGET (self->log_pane));
}

static void
on_edit_config_cb (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  GbpBuilduiWorkspaceAddin *self = user_data;
  IdeConfigManager *config_manager;
  IdeConfig *config;
  IdeContext *context;
  const gchar *id;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  ide_workspace_set_visible_surface_name (self->workspace, "buildui");

  context = ide_widget_get_context (GTK_WIDGET (self->workspace));
  config_manager = ide_config_manager_from_context (context);
  id = g_variant_get_string (param, NULL);
  config = ide_config_manager_get_config (config_manager, id);

  if (config != NULL)
    gbp_buildui_config_surface_set_config (self->surface, config);
}

static const GActionEntry actions[] = {
  { "edit-config", on_edit_config_cb, "s" },
  { "view-output", on_view_output_cb },
};

static void
gbp_buildui_workspace_addin_build_started (GbpBuilduiWorkspaceAddin *self,
                                           IdePipeline              *pipeline,
                                           IdeBuildManager          *build_manager)
{
  IdePipelinePhase phase;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  phase = ide_pipeline_get_requested_phase (pipeline);

  IDE_TRACE_MSG ("Pipeline phase 0x%x requested", phase);

  if (phase > IDE_PIPELINE_PHASE_CONFIGURE)
    dzl_dock_item_present (DZL_DOCK_ITEM (self->log_pane));

  IDE_EXIT;
}

static void
gbp_buildui_workspace_addin_load (IdeWorkspaceAddin *addin,
                                  IdeWorkspace      *workspace)
{
  GbpBuilduiWorkspaceAddin *self = (GbpBuilduiWorkspaceAddin *)addin;
  IdeConfigManager *config_manager;
  PangoAttrList *small_attrs = NULL;
  DzlShortcutController *shortcuts;
  IdeEditorSidebar *sidebar;
  IdeBuildManager *build_manager;
  IdeWorkbench *workbench;
  IdeHeaderBar *headerbar;
  IdeSurface *surface;
  IdeOmniBar *omnibar;
  IdeContext *context;
  GtkWidget *utilities;

  g_assert (GBP_IS_BUILDUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  self->workspace = workspace;

  g_action_map_add_action_entries (G_ACTION_MAP (workspace),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  shortcuts = dzl_shortcut_controller_find (GTK_WIDGET (workspace));
  dzl_shortcut_controller_add_command_action (shortcuts,
                                              "org.gnome.builder.buildui.build",
                                              "<Control>F7",
                                              DZL_SHORTCUT_PHASE_GLOBAL,
                                              "build-manager.build");
  dzl_shortcut_controller_add_command_action (shortcuts,
                                              "org.gnome.builder.buildui.rebuild",
                                              "<Control><Shift>F7",
                                              DZL_SHORTCUT_PHASE_GLOBAL,
                                              "build-manager.rebuild");

  headerbar = ide_workspace_get_header_bar (workspace);
  omnibar = IDE_OMNI_BAR (gtk_header_bar_get_custom_title (GTK_HEADER_BAR (headerbar)));
  workbench = ide_widget_get_workbench (GTK_WIDGET (workspace));
  context = ide_workbench_get_context (workbench);
  build_manager = ide_build_manager_from_context (context);
  config_manager = ide_config_manager_from_context (context);

  small_attrs = pango_attr_list_new ();
  pango_attr_list_insert (small_attrs, pango_attr_scale_new (0.833333));

  self->diag_box = g_object_new (GTK_TYPE_BOX,
                                 "orientation", GTK_ORIENTATION_HORIZONTAL,
                                 "visible", TRUE,
                                 NULL);
  g_signal_connect (self->diag_box,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->diag_box);
  ide_omni_bar_add_status_icon (omnibar, GTK_WIDGET (self->diag_box), 0);

  self->error_image = g_object_new (GTK_TYPE_IMAGE,
                                    "icon-name", "dialog-error-symbolic",
                                    "margin-end", 2,
                                    "margin-start", 4,
                                    "pixel-size", 12,
                                    "valign", GTK_ALIGN_BASELINE,
                                    NULL);
  gtk_container_add (GTK_CONTAINER (self->diag_box), GTK_WIDGET (self->error_image));

  self->error_label = g_object_new (GTK_TYPE_LABEL,
                                    "attributes", small_attrs,
                                    "margin-end", 2,
                                    "margin-start", 2,
                                    "valign", GTK_ALIGN_BASELINE,
                                    NULL);
  gtk_container_add (GTK_CONTAINER (self->diag_box), GTK_WIDGET (self->error_label));

  self->warning_image = g_object_new (GTK_TYPE_IMAGE,
                                      "icon-name", "dialog-warning-symbolic",
                                      "margin-end", 2,
                                      "margin-start", 4,
                                      "pixel-size", 12,
                                      "valign", GTK_ALIGN_BASELINE,
                                      NULL);
  gtk_container_add (GTK_CONTAINER (self->diag_box), GTK_WIDGET (self->warning_image));

  self->warning_label = g_object_new (GTK_TYPE_LABEL,
                                      "attributes", small_attrs,
                                      "margin-end", 2,
                                      "margin-start", 2,
                                      "valign", GTK_ALIGN_BASELINE,
                                      NULL);
  gtk_container_add (GTK_CONTAINER (self->diag_box), GTK_WIDGET (self->warning_label));

  g_clear_pointer (&small_attrs, pango_attr_list_unref);

  self->omni_bar_section = g_object_new (GBP_TYPE_BUILDUI_OMNI_BAR_SECTION,
                                         "visible", TRUE,
                                         NULL);
  g_signal_connect (self->omni_bar_section,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->omni_bar_section);
  ide_omni_bar_add_popover_section (omnibar, GTK_WIDGET (self->omni_bar_section), 0);
  gbp_buildui_omni_bar_section_set_context (self->omni_bar_section, context);

  self->build_button = g_object_new (GTK_TYPE_BUTTON,
                                     "action-name", "build-manager.build",
                                     "child", g_object_new (GTK_TYPE_IMAGE,
                                                            "icon-name", "builder-build-symbolic",
                                                            "visible", TRUE,
                                                            NULL),
                                     "focus-on-click", FALSE,
                                     "has-tooltip", TRUE,
                                     "visible", TRUE,
                                     NULL);
  ide_omni_bar_add_button (omnibar, GTK_WIDGET (self->build_button), GTK_PACK_END, 0);

  self->cancel_button = g_object_new (GTK_TYPE_BUTTON,
                                      "action-name", "build-manager.cancel",
                                      "child", g_object_new (GTK_TYPE_IMAGE,
                                                             "icon-name", "process-stop-symbolic",
                                                             "visible", TRUE,
                                                             NULL),
                                      "focus-on-click", FALSE,
                                      "has-tooltip", TRUE,
                                      "visible", TRUE,
                                      NULL);
  ide_omni_bar_add_button (omnibar, GTK_WIDGET (self->cancel_button), GTK_PACK_END, 0);

  surface = ide_workspace_get_surface_by_name (workspace, "editor");
  utilities = ide_editor_surface_get_utilities (IDE_EDITOR_SURFACE (surface));
  sidebar = ide_editor_surface_get_sidebar (IDE_EDITOR_SURFACE (surface));

  self->log_pane = g_object_new (GBP_TYPE_BUILDUI_LOG_PANE,
                                 "visible", TRUE,
                                 NULL);
  gtk_container_add (GTK_CONTAINER (utilities), GTK_WIDGET (self->log_pane));

  self->pane = g_object_new (GBP_TYPE_BUILDUI_PANE,
                             "visible", TRUE,
                             NULL);
  ide_editor_sidebar_add_section (sidebar,
                                  "build-issues",
                                  _("Build Issues"),
                                  "builder-build-symbolic",
                                  NULL, NULL,
                                  GTK_WIDGET (self->pane),
                                  100);

  self->surface = g_object_new (GBP_TYPE_BUILDUI_CONFIG_SURFACE,
                                "config-manager", config_manager,
                                "icon-name", "builder-build-configure-symbolic",
                                "title", _("Build Preferences"),
                                "name", "buildui",
                                "visible", TRUE,
                                NULL);
  g_signal_connect (self->surface,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->surface);
  ide_workspace_add_surface (workspace, IDE_SURFACE (self->surface));

  self->build_manager_signals = dzl_signal_group_new (IDE_TYPE_BUILD_MANAGER);
  g_signal_connect_object (self->build_manager_signals,
                           "bind",
                           G_CALLBACK (gbp_buildui_workspace_addin_bind_build_manager),
                           self,
                           G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->build_manager_signals,
                                   "notify::error-count",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_error_count),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->build_manager_signals,
                                   "notify::warning-count",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_warning_count),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->build_manager_signals,
                                   "notify::pipeline",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_pipeline),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->build_manager_signals,
                                   "notify::busy",
                                   G_CALLBACK (gbp_buildui_workspace_addin_notify_busy),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->build_manager_signals,
                                   "build-started",
                                   G_CALLBACK (gbp_buildui_workspace_addin_build_started),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_set_target (self->build_manager_signals, build_manager);
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
    gtk_widget_destroy (GTK_WIDGET (self->omni_bar_section));

  if (self->diag_box)
    gtk_widget_destroy (GTK_WIDGET (self->diag_box));

  if (self->surface)
    gtk_widget_destroy (GTK_WIDGET (self->surface));

  dzl_signal_group_set_target (self->build_manager_signals, NULL);
  g_clear_object (&self->build_manager_signals);

  self->workspace = NULL;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_buildui_workspace_addin_load;
  iface->unload = gbp_buildui_workspace_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpBuilduiWorkspaceAddin, gbp_buildui_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_buildui_workspace_addin_class_init (GbpBuilduiWorkspaceAddinClass *klass)
{
}

static void
gbp_buildui_workspace_addin_init (GbpBuilduiWorkspaceAddin *self)
{
}
