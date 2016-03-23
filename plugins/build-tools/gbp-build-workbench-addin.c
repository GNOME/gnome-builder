/* gbp-build-workbench-addin.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "egg-binding-group.h"

#include "gbp-build-log-panel.h"
#include "gbp-build-panel.h"
#include "gbp-build-perspective.h"
#include "gbp-build-workbench-addin.h"

struct _GbpBuildWorkbenchAddin
{
  GObject             parent_instance;

  /* Unowned */
  GbpBuildPanel      *panel;
  IdeWorkbench       *workbench;
  GbpBuildLogPanel   *build_log_panel;

  /* Owned */
  EggBindingGroup    *bindings;
  IdeBuildResult     *result;
  GSimpleActionGroup *actions;
  GCancellable       *cancellable;
};

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpBuildWorkbenchAddin, gbp_build_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

enum {
  PROP_0,
  PROP_RESULT,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
gbp_build_workbench_addin_set_result (GbpBuildWorkbenchAddin *self,
                                      IdeBuildResult         *result)
{
  g_return_if_fail (GBP_IS_BUILD_WORKBENCH_ADDIN (self));
  g_return_if_fail (!result || IDE_IS_BUILD_RESULT (result));

  if (g_set_object (&self->result, result))
    {
      egg_binding_group_set_source (self->bindings, result);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RESULT]);
    }
}

static void
gbp_build_workbench_addin_build_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeBuilder *builder = (IdeBuilder *)object;
  g_autoptr(GbpBuildWorkbenchAddin) self = user_data;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUILDER (builder));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  build_result = ide_builder_build_finish (builder, result, &error);

  if (error != NULL)
    g_warning ("%s", error->message);
}

static void
gbp_build_workbench_addin_save_all_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeBuildResult) build_result = NULL;
  struct {
    GbpBuildWorkbenchAddin *self;
    IdeBuilder *builder;
    IdeBuilderBuildFlags flags;
  } *state = user_data;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (state->self));

  ide_buffer_manager_save_all_finish (bufmgr, result, NULL);

  ide_builder_build_async (state->builder,
                           state->flags,
                           &build_result,
                           state->self->cancellable,
                           gbp_build_workbench_addin_build_cb,
                           g_object_ref (state->self));

  gbp_build_workbench_addin_set_result (state->self, build_result);
  gbp_build_log_panel_set_result (state->self->build_log_panel, build_result);

  g_object_unref (state->self);
  g_object_unref (state->builder);
  g_slice_free1 (sizeof *state, state);
}

static void
gbp_build_workbench_addin_do_build (GbpBuildWorkbenchAddin *self,
                                    IdeBuilderBuildFlags    flags)
{
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(GError) error = NULL;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *configuration;
  IdeBuildSystem *build_system;
  IdeWorkbench *workbench;
  IdeContext *context;
  struct {
    GbpBuildWorkbenchAddin *self;
    IdeBuilder *builder;
    IdeBuilderBuildFlags flags;
  } *state;

  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  gbp_build_workbench_addin_set_result (self, NULL);

  workbench = ide_widget_get_workbench (GTK_WIDGET (self->panel));
  context = ide_workbench_get_context (workbench);
  build_system = ide_context_get_build_system (context);
  config_manager = ide_context_get_configuration_manager (context);
  configuration = ide_configuration_manager_get_current (config_manager);

  builder = ide_build_system_get_builder (build_system, configuration, &error);

  if (error != NULL)
    {
      gbp_build_panel_add_error (self->panel, error->message);
      return;
    }

  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  state = g_slice_alloc0 (sizeof *state);
  state->self = g_object_ref (self);
  state->builder = g_object_ref (builder);
  state->flags = flags;

  ide_buffer_manager_save_all_async (ide_context_get_buffer_manager (context),
                                     self->cancellable,
                                     gbp_build_workbench_addin_save_all_cb,
                                     state);

  /* Ensure the build output is visible */
  /* XXX: we might want to find a way to add a "hold" on the panel
   *      visibility so that it can be hidden after a timeout.
   */
  gtk_widget_show (GTK_WIDGET (self->build_log_panel));
  ide_workbench_focus (workbench, GTK_WIDGET (self->build_log_panel));
  ide_workbench_focus (workbench, GTK_WIDGET (self->panel));
}

static void
gbp_build_workbench_addin_build (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  GbpBuildWorkbenchAddin *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  gbp_build_workbench_addin_do_build (self, 0);
}

static void
gbp_build_workbench_addin_rebuild (GSimpleAction *action,
                                   GVariant      *param,
                                   gpointer       user_data)
{
  GbpBuildWorkbenchAddin *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  gbp_build_workbench_addin_do_build (self, IDE_BUILDER_BUILD_FLAGS_FORCE_CLEAN);
}

static void
gbp_build_workbench_addin_clean (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  GbpBuildWorkbenchAddin *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  gbp_build_workbench_addin_do_build (self,
                                      (IDE_BUILDER_BUILD_FLAGS_FORCE_CLEAN |
                                       IDE_BUILDER_BUILD_FLAGS_NO_BUILD));
}

static void
gbp_build_workbench_addin_cancel (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbpBuildWorkbenchAddin *self = user_data;

  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));

  if (self->cancellable)
    g_cancellable_cancel (self->cancellable);
}

static const GActionEntry actions[] = {
  { "build", gbp_build_workbench_addin_build },
  { "rebuild", gbp_build_workbench_addin_rebuild },
  { "clean", gbp_build_workbench_addin_clean },
  { "cancel-build", gbp_build_workbench_addin_cancel },
};

static void
gbp_build_workbench_addin_load (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  IdeConfigurationManager *configuration_manager;
  GbpBuildWorkbenchAddin *self = (GbpBuildWorkbenchAddin *)addin;
  IdeConfiguration *configuration;
  IdePerspective *editor;
  IdeContext *context;
  GtkWidget *build_perspective;
  GtkWidget *pane;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  context = ide_workbench_get_context (workbench);
  configuration_manager = ide_context_get_configuration_manager (context);
  configuration = ide_configuration_manager_get_current (configuration_manager);

  editor = ide_workbench_get_perspective_by_name (workbench, "editor");
  pane = pnl_dock_bin_get_right_edge (PNL_DOCK_BIN (editor));
  self->panel = g_object_new (GBP_TYPE_BUILD_PANEL,
                              "configuration-manager", configuration_manager,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (pane), GTK_WIDGET (self->panel));

  pane = pnl_dock_bin_get_bottom_edge (PNL_DOCK_BIN (editor));
  self->build_log_panel = g_object_new (GBP_TYPE_BUILD_LOG_PANEL, NULL);
  gtk_container_add (GTK_CONTAINER (pane), GTK_WIDGET (self->build_log_panel));

  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "build-tools",
                                  G_ACTION_GROUP (self->actions));

  g_object_bind_property (self, "result", self->panel, "result", 0);

  build_perspective = g_object_new (GBP_TYPE_BUILD_PERSPECTIVE,
                                    "configuration-manager", configuration_manager,
                                    "configuration", configuration,
                                    "visible", TRUE,
                                    NULL);
  ide_workbench_add_perspective (workbench, IDE_PERSPECTIVE (build_perspective));
}

static void
gbp_build_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpBuildWorkbenchAddin *self = (GbpBuildWorkbenchAddin *)addin;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (GBP_IS_BUILD_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  if (self->cancellable)
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);

  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "build-tools", NULL);

  gtk_widget_destroy (GTK_WIDGET (self->panel));
  self->panel = NULL;
}

static void
gbp_build_workbench_addin_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpBuildWorkbenchAddin *self = GBP_BUILD_WORKBENCH_ADDIN(object);

  switch (prop_id)
    {
    case PROP_RESULT:
      g_value_set_object (value, self->result);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_build_workbench_addin_finalize (GObject *object)
{
  GbpBuildWorkbenchAddin *self = (GbpBuildWorkbenchAddin *)object;

  g_clear_object (&self->bindings);
  g_clear_object (&self->actions);
  g_clear_object (&self->result);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (gbp_build_workbench_addin_parent_class)->finalize (object);
}

static void
gbp_build_workbench_addin_class_init (GbpBuildWorkbenchAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_build_workbench_addin_finalize;
  object_class->get_property = gbp_build_workbench_addin_get_property;

  properties [PROP_RESULT] =
    g_param_spec_object ("result",
                         "Result",
                         "The current build result",
                         IDE_TYPE_BUILD_RESULT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gbp_build_workbench_addin_init (GbpBuildWorkbenchAddin *self)
{
  gint i;
  static const struct {
    const gchar   *property;
    const gchar   *action;
    GBindingFlags  flags;
  } bindings[] = {
    { "running", "build", G_BINDING_INVERT_BOOLEAN },
    { "running", "rebuild", G_BINDING_INVERT_BOOLEAN },
    { "running", "clean", G_BINDING_INVERT_BOOLEAN },
    { "running", "cancel-build", 0 },
    { NULL }
  };

  self->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions, G_N_ELEMENTS (actions),
                                   self);

  self->bindings = egg_binding_group_new ();

  for (i = 0; bindings [i].property; i++)
    {
      GActionMap *map = G_ACTION_MAP (self->actions);
      GAction *action;

      action = g_action_map_lookup_action (map, bindings [i].action);
      egg_binding_group_bind (self->bindings, bindings [i].property,
                              action, "enabled",
                              G_BINDING_SYNC_CREATE | bindings [i].flags);
    }
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_build_workbench_addin_load;
  iface->unload = gbp_build_workbench_addin_unload;
}
