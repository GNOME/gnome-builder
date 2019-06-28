/* gbp-buildui-tree-addin.c
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

#define G_LOG_DOMAIN "gbp-buildui-tree-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-plugins.h>
#include <libide-threading.h>
#include <libide-tree.h>

#include "gbp-buildui-tree-addin.h"

struct _GbpBuilduiTreeAddin
{
  GObject       parent_instance;
  IdeTree      *tree;
  IdeTreeModel *model;
};

typedef struct
{
  IdeExtensionSetAdapter *set;
  IdeTreeNode            *node;
  guint                   n_active;
} BuildTargets;

static void
build_targets_free (BuildTargets *state)
{
  g_clear_object (&state->node);
  ide_clear_and_destroy_object (&state->set);
  g_slice_free (BuildTargets, state);
}

static void
get_targets_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  IdeBuildTargetProvider *provider = (IdeBuildTargetProvider *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) targets = NULL;
  BuildTargets *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUILD_TARGET_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  targets = ide_build_target_provider_get_targets_finish (provider, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (targets, g_object_unref);

  if (targets)
    {
      for (guint i = 0; i < targets->len; i++)
        {
          IdeBuildTarget *target = g_ptr_array_index (targets, i);
          g_autoptr(IdeTreeNode) node = NULL;
          g_autofree gchar *name = NULL;

          name = ide_build_target_get_display_name (target);
          node = g_object_new (IDE_TYPE_TREE_NODE,
                               "destroy-item", TRUE,
                               "display-name", name,
                               "icon-name", "builder-build-symbolic",
                               "item", target,
                               "use-markup", TRUE,
                               NULL);
          ide_tree_node_append (state->node, node);
        }
    }

  state->n_active--;

  if (state->n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

static void
build_targets_cb (IdeExtensionSetAdapter *set,
                  PeasPluginInfo         *plugin_info,
                  PeasExtension          *exten,
                  gpointer                user_data)
{
  IdeBuildTargetProvider *provider = (IdeBuildTargetProvider *)exten;
  IdeTask *task = user_data;
  BuildTargets *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (IDE_IS_BUILD_TARGET_PROVIDER (provider));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);
  state->n_active++;

  ide_build_target_provider_get_targets_async (provider,
                                               ide_task_get_cancellable (task),
                                               get_targets_cb,
                                               g_object_ref (task));
}

static void
gbp_buildui_tree_addin_build_children_async (IdeTreeAddin        *addin,
                                             IdeTreeNode         *node,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  GbpBuilduiTreeAddin *self = (GbpBuilduiTreeAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_buildui_tree_addin_build_children_async);

  context = ide_object_get_context (IDE_OBJECT (self->model));

  if (!ide_context_has_project (context))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  if (ide_tree_node_holds (node, IDE_TYPE_CONTEXT))
    {
      g_autoptr(IdeTreeNode) targets = NULL;

      targets = g_object_new (IDE_TYPE_TREE_NODE,
                              "icon-name", "builder-build-symbolic",
                              "is-header", TRUE,
                              "item", NULL,
                              "display-name", _("Build Targets"),
                              "children-possible", TRUE,
                              "tag", "BUILD_TARGETS",
                              NULL);
      ide_tree_node_prepend (node, targets);
    }
  else if (ide_tree_node_is_tag (node, "BUILD_TARGETS"))
    {
      BuildTargets *state;

      state = g_slice_new0 (BuildTargets);
      state->node = g_object_ref (node);
      state->n_active = 0;
      state->set = ide_extension_set_adapter_new (IDE_OBJECT (self->model),
                                                  peas_engine_get_default (),
                                                  IDE_TYPE_BUILD_TARGET_PROVIDER,
                                                  NULL, NULL);
      ide_task_set_task_data (task, state, build_targets_free);

      ide_extension_set_adapter_foreach (state->set, build_targets_cb, task);

      if (state->n_active > 0)
        return;
    }

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_buildui_tree_addin_build_children_finish (IdeTreeAddin  *addin,
                                              GAsyncResult  *result,
                                              GError       **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_buildui_tree_addin_action_build (GSimpleAction *action,
                                     GVariant      *param,
                                     gpointer       user_data)
{
  GbpBuilduiTreeAddin *self = user_data;
  g_autoptr(GPtrArray) targets = NULL;
  IdeBuildManager *build_manager;
  IdeBuildTarget *target;
  IdeTreeNode *node;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILDUI_TREE_ADDIN (self));

  if (!(context = ide_widget_get_context (GTK_WIDGET (self->tree))) ||
      !(build_manager = ide_build_manager_from_context (context)) ||
      !(node = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (node, IDE_TYPE_BUILD_TARGET) ||
      !(target = ide_tree_node_get_item (node)))
    return;

  targets = g_ptr_array_new_full (1, g_object_unref);
  g_ptr_array_add (targets, g_object_ref (target));

  ide_build_manager_build_async (build_manager, IDE_PIPELINE_PHASE_BUILD, targets, NULL, NULL, NULL);
}

static void
gbp_buildui_tree_addin_action_rebuild (GSimpleAction *action,
                                       GVariant      *param,
                                       gpointer       user_data)
{
  GbpBuilduiTreeAddin *self = user_data;
  g_autoptr(GPtrArray) targets = NULL;
  IdeBuildManager *build_manager;
  IdeBuildTarget *target;
  IdeTreeNode *node;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILDUI_TREE_ADDIN (self));

  if (!(context = ide_widget_get_context (GTK_WIDGET (self->tree))) ||
      !(build_manager = ide_build_manager_from_context (context)) ||
      !(node = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (node, IDE_TYPE_BUILD_TARGET) ||
      !(target = ide_tree_node_get_item (node)))
    return;

  targets = g_ptr_array_new_full (1, g_object_unref);
  g_ptr_array_add (targets, g_object_ref (target));

  ide_build_manager_rebuild_async (build_manager, IDE_PIPELINE_PHASE_BUILD, targets, NULL, NULL, NULL);
}

static void
gbp_buildui_tree_addin_action_run (GSimpleAction *action,
                                   GVariant      *param,
                                   gpointer       user_data)
{
  GbpBuilduiTreeAddin *self = user_data;
  g_autoptr(GPtrArray) targets = NULL;
  IdeBuildManager *build_manager;
  IdeBuildTarget *target;
  IdeRunManager *run_manager;
  IdeTreeNode *node;
  IdeContext *context;
  const gchar *handler;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));
  g_assert (GBP_IS_BUILDUI_TREE_ADDIN (self));

  if (!(context = ide_widget_get_context (GTK_WIDGET (self->tree))) ||
      !(build_manager = ide_build_manager_from_context (context)) ||
      !(node = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (node, IDE_TYPE_BUILD_TARGET) ||
      !(target = ide_tree_node_get_item (node)))
    return;

  run_manager = ide_run_manager_from_context (context);
  handler = g_variant_get_string (param, NULL);

  if (ide_str_empty0 (handler))
    ide_run_manager_set_handler (run_manager, NULL);
  else
    ide_run_manager_set_handler (run_manager, handler);

  ide_run_manager_run_async (run_manager,
                             target,
                             NULL,
                             NULL,
                             NULL);
}

static void
gbp_buildui_tree_addin_load (IdeTreeAddin *addin,
                             IdeTree      *tree,
                             IdeTreeModel *model)
{
  GbpBuilduiTreeAddin *self = (GbpBuilduiTreeAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;
  IdeContext *context;
  static const GActionEntry actions[] = {
    { "build", gbp_buildui_tree_addin_action_build },
    { "rebuild", gbp_buildui_tree_addin_action_rebuild },
    { "run-with-handler", gbp_buildui_tree_addin_action_run, "s" },
  };

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_MODEL (model));

  self->model = model;
  self->tree = tree;

  context = ide_object_get_context (IDE_OBJECT (self->model));

  if (!ide_context_has_project (context))
    return;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (tree), "buildui", G_ACTION_GROUP (group));
}

static void
gbp_buildui_tree_addin_unload (IdeTreeAddin *addin,
                               IdeTree      *tree,
                               IdeTreeModel *model)
{
  GbpBuilduiTreeAddin *self = (GbpBuilduiTreeAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_MODEL (model));

  gtk_widget_insert_action_group (GTK_WIDGET (tree), "buildui", NULL);

  self->model = NULL;
  self->tree = NULL;
}

static void
gbp_buildui_tree_addin_selection_changed (IdeTreeAddin *addin,
                                          IdeTreeNode  *node)
{
  GbpBuilduiTreeAddin *self = (GbpBuilduiTreeAddin *)addin;
  IdeBuildTarget *target;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_BUILDUI_TREE_ADDIN (self));
  g_assert (!node || IDE_IS_TREE_NODE (node));

  context = ide_object_get_context (IDE_OBJECT (self->model));

  if (!ide_context_has_project (context))
    return;

  dzl_gtk_widget_action_set (GTK_WIDGET (self->tree), "buildui", "build",
                             "enabled", node && ide_tree_node_holds (node, IDE_TYPE_BUILD_TARGET),
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self->tree), "buildui", "rebuild",
                             "enabled", node && ide_tree_node_holds (node, IDE_TYPE_BUILD_TARGET),
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self->tree), "buildui", "run-with-handler",
                             "enabled", node &&
                                        ide_tree_node_holds (node, IDE_TYPE_BUILD_TARGET) &&
                                        (target = ide_tree_node_get_item (node)) &&
                                        ide_build_target_get_install (target) &&
                                        ide_build_target_get_kind (target) == IDE_ARTIFACT_KIND_EXECUTABLE,
                             NULL);
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->build_children_async = gbp_buildui_tree_addin_build_children_async;
  iface->build_children_finish = gbp_buildui_tree_addin_build_children_finish;
  iface->selection_changed = gbp_buildui_tree_addin_selection_changed;
  iface->load = gbp_buildui_tree_addin_load;
  iface->unload = gbp_buildui_tree_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpBuilduiTreeAddin, gbp_buildui_tree_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TREE_ADDIN, tree_addin_iface_init))

static void
gbp_buildui_tree_addin_class_init (GbpBuilduiTreeAddinClass *klass)
{
}

static void
gbp_buildui_tree_addin_init (GbpBuilduiTreeAddin *self)
{
}
