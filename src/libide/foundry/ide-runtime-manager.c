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
#include <libpeas.h>

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
  GtkFlattenListModel    *runtimes;
  guint                   unloading : 1;
};

typedef struct
{
  const gchar        *runtime_id;
  IdeRuntimeProvider *provider;
} InstallLookup;

static void list_model_iface_init (GListModelInterface *iface);
static void initable_iface_init   (GInitableIface      *iface);

G_DEFINE_TYPE_EXTENDED (IdeRuntimeManager, ide_runtime_manager, IDE_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

static void
ide_runtime_manager_items_changed_cb (IdeRuntimeManager *self,
                                      guint              position,
                                      guint              removed,
                                      guint              added,
                                      GListModel        *model)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUNTIME_MANAGER (self));
  g_assert (GTK_IS_FLATTEN_LIST_MODEL (model));

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);

  IDE_EXIT;
}

static DexFuture *
ide_runtime_manager_provider_load_cb (DexFuture *future,
                                      gpointer   user_data)
{
  IdeRuntimeProvider *provider = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (DEX_IS_FUTURE (future));
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));

  if (!dex_await (dex_ref (future), &error))
    g_debug ("Runtime provider \"%s\" failed to load with error: %s",
             G_OBJECT_TYPE_NAME (provider), error->message);
  else
    g_debug ("Runtime provider \"%s\" loaded",
             G_OBJECT_TYPE_NAME (provider));

  IDE_RETURN (NULL);
}

static void
ide_runtime_manager_extension_added (IdeExtensionSetAdapter *set,
                                     PeasPluginInfo         *plugin_info,
                                     GObject          *exten,
                                     gpointer                user_data)
{
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)exten;
  IdeRuntimeManager *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));
  g_assert (IDE_IS_RUNTIME_MANAGER (self));

  dex_future_disown (dex_future_finally (ide_runtime_provider_load (provider),
                                         ide_runtime_manager_provider_load_cb,
                                         g_object_ref (provider),
                                         g_object_unref));

  IDE_EXIT;
}

static DexFuture *
ide_runtime_manager_provider_unload_cb (DexFuture *future,
                                        gpointer   user_data)
{
  IdeRuntimeProvider *provider = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (DEX_IS_FUTURE (future));
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));

  if (!dex_await (dex_ref (future), &error))
    g_debug ("Runtime provider \"%s\" failed to unload with error: %s",
             G_OBJECT_TYPE_NAME (provider), error->message);
  else
    g_debug ("Runtime provider \"%s\" unloaded",
             G_OBJECT_TYPE_NAME (provider));

  ide_object_destroy (IDE_OBJECT (provider));

  IDE_RETURN (NULL);
}

static void
ide_runtime_manager_extension_removed (IdeExtensionSetAdapter *set,
                                       PeasPluginInfo         *plugin_info,
                                       GObject          *exten,
                                       gpointer                user_data)
{
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)exten;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));

  dex_future_disown (dex_future_finally (ide_runtime_provider_unload (provider),
                                         ide_runtime_manager_provider_unload_cb,
                                         g_object_ref (provider),
                                         g_object_unref));

  IDE_EXIT;
}

static gboolean
ide_runtime_manager_initable_init (GInitable     *initable,
                                   GCancellable  *cancellable,
                                   GError       **error)
{
  IdeRuntimeManager *self = (IdeRuntimeManager *)initable;

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

  gtk_flatten_list_model_set_model (self->runtimes,
                                    G_LIST_MODEL (self->extensions));

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

  if (self->runtimes != NULL)
    {
      gtk_flatten_list_model_set_model (self->runtimes, NULL);
      g_clear_object (&self->runtimes);
    }

  ide_clear_and_destroy_object (&self->extensions);

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
  self->runtimes = gtk_flatten_list_model_new (NULL);

  g_signal_connect_object (self->runtimes,
                           "items-changed",
                           G_CALLBACK (ide_runtime_manager_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static GType
ide_runtime_manager_get_item_type (GListModel *model)
{
  return IDE_TYPE_RUNTIME;
}

static guint
ide_runtime_manager_get_n_items (GListModel *model)
{
  IdeRuntimeManager *self = IDE_RUNTIME_MANAGER (model);

  if (self->runtimes != NULL)
    return g_list_model_get_n_items (G_LIST_MODEL (self->runtimes));

  return 0;
}

static gpointer
ide_runtime_manager_get_item (GListModel *model,
                              guint       position)
{
  IdeRuntimeManager *self = IDE_RUNTIME_MANAGER (model);

  if (self->runtimes != NULL)
    return g_list_model_get_item (G_LIST_MODEL (self->runtimes), position);

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_runtime_manager_get_item_type;
  iface->get_n_items = ide_runtime_manager_get_n_items;
  iface->get_item = ide_runtime_manager_get_item;
}

/**
 * ide_runtime_manager_get_runtime:
 * @self: An #IdeRuntimeManager
 * @id: the identifier of the runtime
 *
 * Gets the runtime by its internal identifier.
 *
 * Returns: (nullable) (transfer none): An #IdeRuntime.
 */
IdeRuntime *
ide_runtime_manager_get_runtime (IdeRuntimeManager *self,
                                 const gchar       *id)
{
  IdeRuntime *ret = NULL;
  guint n_items;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  /* TODO: Rename and fix transfer semantics to ref_runtime(). */

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeRuntime) runtime = g_list_model_get_item (G_LIST_MODEL (self), i);
      const gchar *runtime_id = ide_runtime_get_id (runtime);

      if (g_strcmp0 (runtime_id, id) == 0)
        {
          /* Return borrowed pointer */
          ret = runtime;
          break;
        }
    }

  return ret;
}

static void
provides_lookup_cb (IdeExtensionSetAdapter *set,
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
      if (ide_runtime_provider_provides (provider, lookup->runtime_id))
        lookup->provider = provider;
    }
}

typedef struct _Prepare
{
  IdeRuntimeManager *self;
  IdePipeline       *pipeline;
} Prepare;

static void
prepare_free (Prepare *prepare)
{
  g_clear_object (&prepare->self);
  g_clear_object (&prepare->pipeline);
  g_free (prepare);
}

static Prepare *
prepare_new (IdeRuntimeManager *self,
             IdePipeline       *pipeline)
{
  Prepare *prepare = g_new0 (Prepare, 1);

  g_set_object (&prepare->self, self);
  g_set_object (&prepare->pipeline, pipeline);

  return prepare;
}

static DexFuture *
ide_runtime_manager_prepare_fiber (gpointer user_data)
{
  g_autoptr(IdeRuntime) resolved = NULL;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(GError) error = NULL;
  Prepare *prepare = user_data;
  g_autofree char *runtime_id = NULL;
  IdeRuntime *runtime = NULL;
  IdeConfig *config;
  InstallLookup lookup;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (prepare != NULL);
  g_assert (IDE_IS_RUNTIME_MANAGER (prepare->self));
  g_assert (IDE_IS_PIPELINE (prepare->pipeline));

  config = ide_pipeline_get_config (prepare->pipeline);
  runtime_id = g_strdup (ide_config_get_runtime_id (config));

  /*
   * Detect extensions that are a runtime-provider for the configured
   * runtime_id.  Providers might need more time to finish setting up, but they
   * can indicate here that they do provide the runtime for the current
   * runtime_id. The runtime can then use the bootstrap_async method to finish
   * the setup and let us know when it's ready.
   */
  lookup = (InstallLookup) { .runtime_id = runtime_id };
  ide_extension_set_adapter_foreach (prepare->self->extensions,
                                     (IdeExtensionSetAdapterForeachFunc) provides_lookup_cb,
                                     &lookup);

  if (lookup.provider == NULL)
    future = dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_NOT_SUPPORTED,
                                    "Failed to locate provider for runtime: %s",
                                    runtime_id);
  else
    future = ide_runtime_provider_bootstrap_runtime (lookup.provider, prepare->pipeline);

  if ((resolved = dex_await_object (g_steal_pointer (&future), &error)))
    _ide_pipeline_set_runtime (prepare->pipeline, resolved);
  else if ((runtime = ide_runtime_manager_get_runtime (prepare->self, runtime_id)))
    _ide_pipeline_set_runtime (prepare->pipeline, runtime);

  if (resolved != NULL || runtime != NULL)
    future = dex_future_new_for_boolean (TRUE);
  else
    future = dex_future_new_for_error (g_steal_pointer (&error));

  IDE_RETURN (g_steal_pointer (&future));
}

static DexFuture *
ide_runtime_manager_prepare (IdeRuntimeManager *self,
                             IdePipeline       *pipeline)
{
  g_autoptr(DexFuture) ret = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUNTIME_MANAGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  ret = dex_scheduler_spawn (NULL, 0,
                             ide_runtime_manager_prepare_fiber,
                             prepare_new (self, pipeline),
                             (GDestroyNotify)prepare_free);

  IDE_RETURN (g_steal_pointer (&ret));
}

void
_ide_runtime_manager_prepare_async (IdeRuntimeManager   *self,
                                    IdePipeline         *pipeline,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(DexAsyncResult) result = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  result = dex_async_result_new (self, cancellable, callback, user_data);
  dex_async_result_await (result,
                          ide_runtime_manager_prepare (self, pipeline));

  IDE_EXIT;
}

gboolean
_ide_runtime_manager_prepare_finish (IdeRuntimeManager  *self,
                                     GAsyncResult       *result,
                                     GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  ret = dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);

  IDE_RETURN (ret);
}
