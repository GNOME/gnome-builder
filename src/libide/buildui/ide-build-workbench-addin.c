/* ide-build-workbench-addin.c
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-workbench-addin"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "buildui/ide-build-log-panel.h"
#include "buildui/ide-build-panel.h"
#include "buildui/ide-build-perspective.h"
#include "buildui/ide-build-workbench-addin.h"
#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-sidebar.h"

struct _IdeBuildWorkbenchAddin
{
  GObject              parent_instance;

  /* Unowned */
  IdeBuildPanel       *panel;
  IdeWorkbench        *workbench;
  IdeBuildLogPanel    *build_log_panel;
  GtkWidget           *run_button;
  IdeBuildPerspective *build_perspective;

  /* Owned */
  IdeBuildPipeline    *pipeline;
  GSimpleActionGroup  *actions;
};

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeBuildWorkbenchAddin, ide_build_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

enum {
  PROP_0,
  PROP_PIPELINE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_build_workbench_addin_set_pipeline (IdeBuildWorkbenchAddin *self,
                                        IdeBuildPipeline       *pipeline)
{
  g_return_if_fail (IDE_IS_BUILD_WORKBENCH_ADDIN (self));
  g_return_if_fail (!pipeline || IDE_IS_BUILD_PIPELINE (pipeline));
  g_return_if_fail (self->workbench != NULL);

  if (g_set_object (&self->pipeline, pipeline))
    {
      ide_build_log_panel_set_pipeline (self->build_log_panel, pipeline);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PIPELINE]);
    }

  if (pipeline != NULL)
    {
      gtk_widget_show (GTK_WIDGET (self->build_log_panel));

      if (ide_build_pipeline_get_requested_phase (pipeline) >= IDE_BUILD_PHASE_BUILD)
        dzl_dock_item_present (DZL_DOCK_ITEM (self->build_log_panel));
    }
}

static void
ide_build_workbench_addin_view_output (GSimpleAction *action,
                                       GVariant      *param,
                                       gpointer       user_data)
{
  IdeBuildWorkbenchAddin *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_BUILD_WORKBENCH_ADDIN (self));

  ide_workbench_focus (self->workbench, GTK_WIDGET (self->build_log_panel));
}

static void
ide_build_workbench_addin_configure (GSimpleAction *action,
                                     GVariant      *param,
                                     gpointer       user_data)
{
  IdeBuildWorkbenchAddin *self = user_data;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *config;
  IdeContext *context;
  const gchar *id;

  g_assert (IDE_IS_BUILD_WORKBENCH_ADDIN (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  ide_workbench_set_visible_perspective (self->workbench,
                                         IDE_PERSPECTIVE (self->build_perspective));

  context = ide_workbench_get_context (self->workbench);
  config_manager = ide_context_get_configuration_manager (context);
  id = g_variant_get_string (param, NULL);
  config = ide_configuration_manager_get_configuration (config_manager, id);

  if (config != NULL)
    ide_build_perspective_set_configuration (self->build_perspective, config);
}

static const GActionEntry actions_entries[] = {
  { "configure", ide_build_workbench_addin_configure, "s" },
  { "view-output", ide_build_workbench_addin_view_output },
};

static void
ide_build_workbench_addin_load (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  IdeConfigurationManager *configuration_manager;
  IdeBuildWorkbenchAddin *self = (IdeBuildWorkbenchAddin *)addin;
  IdeConfiguration *configuration;
  IdeEditorSidebar *sidebar;
  IdeBuildManager *build_manager;
  IdePerspective *editor;
  IdeContext *context;
  GtkWidget *pane;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_BUILD_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  context = ide_workbench_get_context (workbench);

  build_manager = ide_context_get_build_manager (context);

  g_signal_connect_object (build_manager,
                           "build-started",
                           G_CALLBACK (ide_build_workbench_addin_set_pipeline),
                           self,
                           G_CONNECT_SWAPPED);

  configuration_manager = ide_context_get_configuration_manager (context);
  configuration = ide_configuration_manager_get_current (configuration_manager);

  editor = ide_workbench_get_perspective_by_name (workbench, "editor");
  sidebar = ide_editor_perspective_get_sidebar (IDE_EDITOR_PERSPECTIVE (editor));

  self->panel = g_object_new (IDE_TYPE_BUILD_PANEL,
                              "visible", TRUE,
                              NULL);
  g_signal_connect (self->panel,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->panel);
  ide_editor_sidebar_add_section (sidebar,
                                  "build-issues",
                                  _("Build Issues"),
                                  "builder-build-symbolic",
                                  NULL, NULL,
                                  GTK_WIDGET (self->panel),
                                  100);

  pane = ide_editor_perspective_get_utilities (IDE_EDITOR_PERSPECTIVE (editor));
  self->build_log_panel = g_object_new (IDE_TYPE_BUILD_LOG_PANEL,
                                        "icon-name", "builder-build-symbolic",
                                        NULL);
  g_signal_connect (self->build_log_panel,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->build_log_panel);
  gtk_container_add (GTK_CONTAINER (pane), GTK_WIDGET (self->build_log_panel));

  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "buildui",
                                  G_ACTION_GROUP (self->actions));

  g_object_bind_property (self, "pipeline", self->panel, "pipeline", 0);

  self->build_perspective = g_object_new (IDE_TYPE_BUILD_PERSPECTIVE,
                                          "configuration-manager", configuration_manager,
                                          "configuration", configuration,
                                          "visible", TRUE,
                                          NULL);
  ide_workbench_add_perspective (workbench, IDE_PERSPECTIVE (self->build_perspective));
}

static void
ide_build_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  IdeBuildWorkbenchAddin *self = (IdeBuildWorkbenchAddin *)addin;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_BUILD_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "buildui", NULL);

  if (self->panel != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->panel));

  if (self->build_log_panel != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->build_log_panel));
}

static void
ide_build_workbench_addin_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeBuildWorkbenchAddin *self = IDE_BUILD_WORKBENCH_ADDIN(object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      g_value_set_object (value, self->pipeline);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_build_workbench_addin_finalize (GObject *object)
{
  IdeBuildWorkbenchAddin *self = (IdeBuildWorkbenchAddin *)object;

  g_clear_object (&self->actions);
  g_clear_object (&self->pipeline);

  G_OBJECT_CLASS (ide_build_workbench_addin_parent_class)->finalize (object);
}

static void
ide_build_workbench_addin_class_init (IdeBuildWorkbenchAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_build_workbench_addin_finalize;
  object_class->get_property = ide_build_workbench_addin_get_property;

  properties [PROP_PIPELINE] =
    g_param_spec_object ("pipeline",
                         "Pipeline",
                         "The current build pipeline",
                         IDE_TYPE_BUILD_PIPELINE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_build_workbench_addin_init (IdeBuildWorkbenchAddin *self)
{
  self->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions_entries,
                                   G_N_ELEMENTS (actions_entries),
                                   self);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = ide_build_workbench_addin_load;
  iface->unload = ide_build_workbench_addin_unload;
}
