/* ide-runtime-manager.c
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

#define G_LOG_DOMAIN "ide-runtime-manager"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-plugins.h>
#include <libide-threading.h>
#include <libpeas/peas.h>

#include "ide-pipeline.h"
#include "ide-build-private.h"
#include "ide-config.h"
#include "ide-device.h"
#include "ide-runtime.h"
#include "ide-runtime-manager.h"
#include "ide-runtime-private.h"
#include "ide-runtime-provider.h"

struct _IdeRuntimeManager
{
  IdeObject               parent_instance;
  IdeExtensionSetAdapter *extensions;
  GPtrArray              *runtimes;
  guint                   unloading : 1;
};

typedef struct
{
  const gchar        *runtime_id;
  IdeRuntimeProvider *provider;
} InstallLookup;

typedef struct
{
  IdePipeline *pipeline;
  gchar            *runtime_id;
} PrepareState;

static void list_model_iface_init (GListModelInterface *iface);
static void initable_iface_init   (GInitableIface      *iface);

G_DEFINE_TYPE_EXTENDED (IdeRuntimeManager, ide_runtime_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

static void
prepare_state_free (PrepareState *state)
{
  g_clear_object (&state->pipeline);
  g_clear_pointer (&state->runtime_id, g_free);
  g_slice_free (PrepareState, state);
}

static void
ide_runtime_manager_extension_added (IdeExtensionSetAdapter *set,
                                     PeasPluginInfo         *plugin_info,
                                     PeasExtension          *exten,
                                     gpointer                user_data)
{
  IdeRuntimeManager *self = user_data;
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)exten;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));

  ide_runtime_provider_load (provider, self);
}

static void
ide_runtime_manager_extension_removed (IdeExtensionSetAdapter *set,
                                       PeasPluginInfo         *plugin_info,
                                       PeasExtension          *exten,
                                       gpointer                user_data)
{
  IdeRuntimeManager *self = user_data;
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)exten;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));

  ide_runtime_provider_unload (provider, self);
}

static gboolean
ide_runtime_manager_initable_init (GInitable     *initable,
                                   GCancellable  *cancellable,
                                   GError       **error)
{
  IdeRuntimeManager *self = (IdeRuntimeManager *)initable;
  g_autoptr(IdeRuntime) host = NULL;

  g_assert (IDE_IS_RUNTIME_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->extensions = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                    peas_engine_get_default (),
                                                    IDE_TYPE_RUNTIME_PROVIDER,
                                                    NULL, NULL);

  g_signal_connect (self->extensions,
                    "extension-added",
                    G_CALLBACK (ide_runtime_manager_extension_added),
                    self);

  g_signal_connect (self->extensions,
                    "extension-removed",
                    G_CALLBACK (ide_runtime_manager_extension_removed),
                    self);

  ide_extension_set_adapter_foreach (self->extensions,
                                     ide_runtime_manager_extension_added,
                                     self);

  host = ide_runtime_new ("host", _("Host Operating System"));
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (host));

  ide_runtime_manager_add (self, host);

  return TRUE;
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_runtime_manager_initable_init;
}

static void
ide_runtime_manager_destroy (IdeObject *object)
{
  IdeRuntimeManager *self = (IdeRuntimeManager *)object;

  self->unloading = TRUE;

  ide_clear_and_destroy_object (&self->extensions);
  g_clear_pointer (&self->runtimes, g_ptr_array_unref);

  IDE_OBJECT_CLASS (ide_runtime_manager_parent_class)->destroy (object);
}

static void
ide_runtime_manager_class_init (IdeRuntimeManagerClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_runtime_manager_destroy;
}

static void
ide_runtime_manager_init (IdeRuntimeManager *self)
{
  self->runtimes = g_ptr_array_new_with_free_func (g_object_unref);
}

static GType
ide_runtime_manager_get_item_type (GListModel *model)
{
  return IDE_TYPE_RUNTIME;
}

static guint
ide_runtime_manager_get_n_items (GListModel *model)
{
  IdeRuntimeManager *self = (IdeRuntimeManager *)model;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), 0);

  return self->runtimes->len;
}

static gpointer
ide_runtime_manager_get_item (GListModel *model,
                              guint       position)
{
  IdeRuntimeManager *self = (IdeRuntimeManager *)model;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), NULL);
  g_return_val_if_fail (position < self->runtimes->len, NULL);

  return g_object_ref (g_ptr_array_index (self->runtimes, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_runtime_manager_get_item_type;
  iface->get_n_items = ide_runtime_manager_get_n_items;
  iface->get_item = ide_runtime_manager_get_item;
}

void
ide_runtime_manager_add (IdeRuntimeManager *self,
                         IdeRuntime        *runtime)
{
  guint idx;

  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (self));
  g_return_if_fail (IDE_IS_RUNTIME (runtime));

  idx = self->runtimes->len;
  g_ptr_array_add (self->runtimes, g_object_ref (runtime));
  g_list_model_items_changed (G_LIST_MODEL (self), idx, 0, 1);
}

void
ide_runtime_manager_remove (IdeRuntimeManager *self,
                            IdeRuntime        *runtime)
{
  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (self));
  g_return_if_fail (IDE_IS_RUNTIME (runtime));

  for (guint i = 0; i < self->runtimes->len; i++)
    {
      IdeRuntime *item = g_ptr_array_index (self->runtimes, i);

      if (runtime == item)
        {
          g_ptr_array_remove_index (self->runtimes, i);
          if (!ide_object_in_destruction (IDE_OBJECT (self)))
            g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          break;
        }
    }
}

/**
 * ide_runtime_manager_get_runtime:
 * @self: An #IdeRuntimeManager
 * @id: the identifier of the runtime
 *
 * Gets the runtime by its internal identifier.
 *
 * Returns: (transfer none): An #IdeRuntime.
 *
 * Since: 3.32
 */
IdeRuntime *
ide_runtime_manager_get_runtime (IdeRuntimeManager *self,
                                 const gchar       *id)
{
  guint i;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  for (i = 0; i < self->runtimes->len; i++)
    {
      IdeRuntime *runtime = g_ptr_array_index (self->runtimes, i);
      const gchar *runtime_id = ide_runtime_get_id (runtime);

      if (g_strcmp0 (runtime_id, id) == 0)
        return runtime;
    }

  return NULL;
}

static void
install_lookup_cb (IdeExtensionSetAdapter *set,
                   PeasPluginInfo         *plugin,
                   IdeRuntimeProvider     *provider,
                   InstallLookup          *lookup)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin != NULL);
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));
  g_assert (lookup != NULL);
  g_assert (lookup->runtime_id != NULL);
  g_assert (lookup->provider == NULL || IDE_IS_RUNTIME_PROVIDER (lookup->provider));

  if (lookup->provider == NULL)
    {
      if (ide_runtime_provider_can_install (provider, lookup->runtime_id))
        lookup->provider = provider;
    }
}

static void
ide_runtime_manager_prepare_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)object;
  g_autoptr(IdeRuntime) runtime = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  runtime = ide_runtime_provider_bootstrap_finish (provider, result, &error);

  g_assert (!runtime ||IDE_IS_RUNTIME (runtime));

  if (runtime == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&runtime), g_object_unref);

  IDE_EXIT;
}

void
_ide_runtime_manager_prepare_async (IdeRuntimeManager   *self,
                                    IdePipeline    *pipeline,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeConfig *config;
  PrepareState *state;
  const gchar *runtime_id;
  InstallLookup lookup = { 0 };

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  config = ide_pipeline_get_config (pipeline);
  runtime_id = ide_config_get_runtime_id (config);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, _ide_runtime_manager_prepare_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_release_on_propagate (task, FALSE);

  state = g_slice_new0 (PrepareState);
  state->runtime_id = g_strdup (runtime_id);
  state->pipeline = g_object_ref (pipeline);
  ide_task_set_task_data (task, state, prepare_state_free);

  if (runtime_id == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Configuration lacks runtime specification");
      IDE_EXIT;
    }

  /*
   * It would be tempting to just return early here if we could locate
   * the runtime as already registered. But that isn't enough since we
   * might need to also install an SDK.
   */

  lookup.runtime_id = runtime_id;
  ide_extension_set_adapter_foreach (self->extensions,
                                     (IdeExtensionSetAdapterForeachFunc) install_lookup_cb,
                                     &lookup);

  if (lookup.provider == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Failed to locate provider for runtime: %s",
                               runtime_id);
  else
    ide_runtime_provider_bootstrap_async (lookup.provider,
                                          pipeline,
                                          cancellable,
                                          ide_runtime_manager_prepare_cb,
                                          g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
_ide_runtime_manager_prepare_finish (IdeRuntimeManager  *self,
                                     GAsyncResult       *result,
                                     GError            **error)
{
  g_autoptr(IdeRuntime) ret = NULL;
  g_autoptr(GError) local_error = NULL;
  PrepareState *state;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  state = ide_task_get_task_data (IDE_TASK (result));
  ret = ide_task_propagate_pointer (IDE_TASK (result), &local_error);

  /*
   * If we got NOT_SUPPORTED error, and the runtime already exists,
   * then we can synthesize a successful result to the caller.
   */
  if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    {
      if ((ret = ide_runtime_manager_get_runtime (self, state->runtime_id)))
        {
          g_object_ref (ret);
          g_clear_error (&local_error);
        }
    }

  if (error != NULL)
    *error = g_steal_pointer (&local_error);

  g_return_val_if_fail (!ret || IDE_IS_RUNTIME (ret), FALSE);

  if (IDE_IS_RUNTIME (ret))
    _ide_pipeline_set_runtime (state->pipeline, ret);

  IDE_RETURN (ret != NULL);
}
