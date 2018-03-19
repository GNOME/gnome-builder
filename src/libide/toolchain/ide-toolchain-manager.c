/* ide-toolchain-manager.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#define G_LOG_DOMAIN "ide-toolchain-manager"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buildsystem/ide-build-private.h"
#include "config/ide-configuration.h"
#include "devices/ide-device.h"
#include "toolchain/ide-toolchain.h"
#include "toolchain/ide-toolchain-manager.h"
#include "toolchain/ide-toolchain-provider.h"

struct _IdeToolchainManager
{
  IdeObject         parent_instance;
  PeasExtensionSet *extensions;
  GPtrArray        *toolchains;
  guint             unloading : 1;
};

typedef struct
{
  IdeBuildPipeline *pipeline;
  gchar            *toolchain_id;
} PrepareState;

static void list_model_iface_init (GListModelInterface *iface);
static void initable_iface_init   (GInitableIface      *iface);

G_DEFINE_TYPE_EXTENDED (IdeToolchainManager, ide_toolchain_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

static void
prepare_state_free (PrepareState *state)
{
  g_clear_object (&state->pipeline);
  g_clear_pointer (&state->toolchain_id, g_free);
  g_slice_free (PrepareState, state);
}

static void
ide_toolchain_manager_extension_added (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       PeasExtension    *exten,
                                       gpointer          user_data)
{
  IdeToolchainManager *self = user_data;
  IdeToolchainProvider *provider = (IdeToolchainProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));

  ide_toolchain_provider_load (provider, self);
}

static void
ide_toolchain_manager_extension_removed (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         PeasExtension    *exten,
                                         gpointer          user_data)
{
  IdeToolchainManager *self = user_data;
  IdeToolchainProvider *provider = (IdeToolchainProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));

  ide_toolchain_provider_unload (provider, self);
}

static gboolean
ide_toolchain_manager_initable_init (GInitable     *initable,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  IdeToolchainManager *self = (IdeToolchainManager *)initable;
  IdeContext *context;

  g_assert (IDE_IS_TOOLCHAIN_MANAGER (self));
  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             IDE_TYPE_TOOLCHAIN_PROVIDER,
                                             NULL);

  g_signal_connect (self->extensions,
                    "extension-added",
                    G_CALLBACK (ide_toolchain_manager_extension_added),
                    self);

  g_signal_connect (self->extensions,
                    "extension-removed",
                    G_CALLBACK (ide_toolchain_manager_extension_removed),
                    self);

  peas_extension_set_foreach (self->extensions,
                              ide_toolchain_manager_extension_added,
                              self);

  ide_toolchain_manager_add (self, ide_toolchain_new (context, "default"));

  return TRUE;
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_toolchain_manager_initable_init;
}

void
_ide_toolchain_manager_unload (IdeToolchainManager *self)
{
  g_return_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self));

  self->unloading = TRUE;
  g_clear_object (&self->extensions);
}

static void
ide_toolchain_manager_dispose (GObject *object)
{
  IdeToolchainManager *self = (IdeToolchainManager *)object;

  _ide_toolchain_manager_unload (self);
  g_clear_pointer (&self->toolchains, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_toolchain_manager_parent_class)->dispose (object);
}

static void
ide_toolchain_manager_class_init (IdeToolchainManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_toolchain_manager_dispose;
}

static void
ide_toolchain_manager_init (IdeToolchainManager *self)
{
  self->toolchains = g_ptr_array_new_with_free_func (g_object_unref);
}

static GType
ide_toolchain_manager_get_item_type (GListModel *model)
{
  return IDE_TYPE_TOOLCHAIN;
}

static guint
ide_toolchain_manager_get_n_items (GListModel *model)
{
  IdeToolchainManager *self = (IdeToolchainManager *)model;

  g_return_val_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self), 0);

  return self->toolchains->len;
}

static gpointer
ide_toolchain_manager_get_item (GListModel *model,
                                guint       position)
{
  IdeToolchainManager *self = (IdeToolchainManager *)model;

  g_return_val_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self), NULL);
  g_return_val_if_fail (position < self->toolchains->len, NULL);

  return g_object_ref (g_ptr_array_index (self->toolchains, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_toolchain_manager_get_item_type;
  iface->get_n_items = ide_toolchain_manager_get_n_items;
  iface->get_item = ide_toolchain_manager_get_item;
}

void
ide_toolchain_manager_add (IdeToolchainManager *self,
                           IdeToolchain        *toolchain)
{
  guint idx;

  g_return_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self));
  g_return_if_fail (IDE_IS_TOOLCHAIN (toolchain));

  idx = self->toolchains->len;
  g_ptr_array_add (self->toolchains, g_object_ref (toolchain));
  g_list_model_items_changed (G_LIST_MODEL (self), idx, 0, 1);
}

void
ide_toolchain_manager_remove (IdeToolchainManager *self,
                              IdeToolchain        *toolchain)
{
  g_return_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self));
  g_return_if_fail (IDE_IS_TOOLCHAIN (toolchain));

  for (guint i = 0; i < self->toolchains->len; i++)
    {
      IdeToolchain *item = g_ptr_array_index (self->toolchains, i);

      if (toolchain == item)
        {
          g_ptr_array_remove_index (self->toolchains, i);
          if (!ide_object_is_unloading (IDE_OBJECT (self)))
            g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          break;
        }
    }
}

/**
 * ide_toolchain_manager_get_toolchain:
 * @self: An #IdeToolchainManager
 * @id: the identifier of the toolchain
 *
 * Gets the toolchain by its internal identifier.
 *
 * Returns: (transfer none): An #IdeToolchain.
 */
IdeToolchain *
ide_toolchain_manager_get_toolchain (IdeToolchainManager *self,
                                     const gchar         *id)
{
  guint i;

  g_return_val_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  for (i = 0; i < self->toolchains->len; i++)
    {
      IdeToolchain *toolchain = g_ptr_array_index (self->toolchains, i);
      const gchar *toolchain_id = ide_toolchain_get_id (toolchain);

      if (g_strcmp0 (toolchain_id, id) == 0)
        return toolchain;
    }

  return NULL;
}

void
_ide_toolchain_manager_prepare_async (IdeToolchainManager  *self,
                                      IdeBuildPipeline     *pipeline,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeConfiguration *config;
  IdeToolchain *toolchain;
  PrepareState *state;
  const gchar *toolchain_id;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  config = ide_build_pipeline_get_configuration (pipeline);
  toolchain_id = ide_configuration_get_toolchain_id (config);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, _ide_toolchain_manager_prepare_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  state = g_slice_new0 (PrepareState);
  state->toolchain_id = g_strdup (toolchain_id);
  state->pipeline = g_object_ref (pipeline);
  g_task_set_task_data (task, state, (GDestroyNotify)prepare_state_free);

  if (toolchain_id == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Configuration lacks toolchain specification");
      IDE_EXIT;
    }

  toolchain = ide_toolchain_manager_get_toolchain (self, toolchain_id);
  if (toolchain == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Configuration toolchain specification does not exist");
      IDE_EXIT;
    }

  g_task_return_pointer (task, g_steal_pointer (&toolchain), g_object_unref);

  IDE_EXIT;
}

gboolean
_ide_toolchain_manager_prepare_finish (IdeToolchainManager  *self,
                                       GAsyncResult         *result,
                                       GError              **error)
{
  g_autoptr(IdeToolchain) ret = NULL;
  g_autoptr(GError) local_error = NULL;
  PrepareState *state;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  state = g_task_get_task_data (G_TASK (result));
  ret = g_task_propagate_pointer (G_TASK (result), &local_error);

  /*
   * If we got NOT_SUPPORTED error, and the toolchain already exists,
   * then we can synthesize a successful result to the caller.
   */
  if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    {
      if ((ret = ide_toolchain_manager_get_toolchain (self, state->toolchain_id)))
        g_clear_error (&local_error);
    }

  if (error != NULL)
    *error = g_steal_pointer (&local_error);

  g_return_val_if_fail (!ret || IDE_IS_TOOLCHAIN (ret), FALSE);

  if (IDE_IS_TOOLCHAIN (ret))
    _ide_build_pipeline_set_toolchain (state->pipeline, ret);

  IDE_RETURN (ret != NULL);
}
