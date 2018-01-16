/* ide-runtime-manager.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-runtime-manager"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buildsystem/ide-configuration.h"
#include "runtimes/ide-runtime.h"
#include "runtimes/ide-runtime-manager.h"
#include "runtimes/ide-runtime-provider.h"

struct _IdeRuntimeManager
{
  IdeObject         parent_instance;
  PeasExtensionSet *extensions;
  GPtrArray        *runtimes;
  guint             unloading : 1;
};

typedef struct
{
  const gchar        *runtime_id;
  IdeRuntimeProvider *provider;
} InstallLookup;

static void list_model_iface_init (GListModelInterface *iface);
static void initable_iface_init   (GInitableIface      *iface);

G_DEFINE_TYPE_EXTENDED (IdeRuntimeManager, ide_runtime_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

static void
ide_runtime_manager_extension_added (PeasExtensionSet *set,
                                     PeasPluginInfo   *plugin_info,
                                     PeasExtension    *exten,
                                     gpointer          user_data)
{
  IdeRuntimeManager *self = user_data;
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));

  ide_runtime_provider_load (provider, self);
}

static void
ide_runtime_manager_extension_removed (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       PeasExtension    *exten,
                                       gpointer          user_data)
{
  IdeRuntimeManager *self = user_data;
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
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
  IdeContext *context;

  g_assert (IDE_IS_RUNTIME_MANAGER (self));
  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             IDE_TYPE_RUNTIME_PROVIDER,
                                             NULL);

  g_signal_connect (self->extensions,
                    "extension-added",
                    G_CALLBACK (ide_runtime_manager_extension_added),
                    self);

  g_signal_connect (self->extensions,
                    "extension-removed",
                    G_CALLBACK (ide_runtime_manager_extension_removed),
                    self);

  peas_extension_set_foreach (self->extensions,
                              ide_runtime_manager_extension_added,
                              self);

  ide_runtime_manager_add (self, ide_runtime_new (context, "host", _("Host operating system")));

  return TRUE;
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_runtime_manager_initable_init;
}

void
_ide_runtime_manager_unload (IdeRuntimeManager *self)
{
  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (self));

  self->unloading = TRUE;
  g_clear_object (&self->extensions);
}

static void
ide_runtime_manager_dispose (GObject *object)
{
  IdeRuntimeManager *self = (IdeRuntimeManager *)object;

  _ide_runtime_manager_unload (self);
  g_clear_pointer (&self->runtimes, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_runtime_manager_parent_class)->dispose (object);
}

static void
ide_runtime_manager_class_init (IdeRuntimeManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_runtime_manager_dispose;
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

  if (self->unloading)
    return;

  for (guint i = 0; i < self->runtimes->len; i++)
    {
      IdeRuntime *item = g_ptr_array_index (self->runtimes, i);

      if (runtime == item)
        {
          g_ptr_array_remove_index (self->runtimes, i);
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
install_lookup_cb (PeasExtensionSet   *set,
                   PeasPluginInfo     *plugin,
                   IdeRuntimeProvider *provider,
                   InstallLookup      *lookup)
{
  g_assert (PEAS_IS_EXTENSION_SET (set));
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
ide_runtime_manager_install_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)object;
  IdeRuntimeManager *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeRuntime *runtime;
  const gchar *runtime_id;

  IDE_ENTRY;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_runtime_provider_install_finish (provider, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self = g_task_get_source_object (task);
  runtime_id = g_task_get_task_data (task);

  g_assert (IDE_IS_RUNTIME_MANAGER (self));
  g_assert (runtime_id != NULL);

  runtime = ide_runtime_manager_get_runtime (self, runtime_id);

  if (runtime == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Runtime provider returned success but did not register runtime %s",
                               runtime_id);
      IDE_EXIT;
    }

  g_task_return_pointer (task, g_object_ref (runtime), g_object_unref);

  IDE_EXIT;
}

/**
 * ide_runtime_manager_ensure_async:
 * @self: An #IdeRuntimeManager
 * @runtime_id: the id for an expected runtime
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to call after execution
 * @user_data: user data for @callback
 *
 * This function will asynchronously check if a runtime is installed.
 *
 * If it is not installed, it will check to see if any runtime provider
 * can provide the runtime by installing it. If so, the runtime will be
 * installed.
 *
 * Call ide_runtime_manager_ensure_finish() to get the resulting runtime
 * or a #GError in case of failure.
 */
void
ide_runtime_manager_ensure_async (IdeRuntimeManager   *self,
                                  const gchar         *runtime_id,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  InstallLookup lookup = {
    .runtime_id = runtime_id,
    .provider = NULL
  };

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (self));
  g_return_if_fail (runtime_id != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_runtime_manager_ensure_async);
  g_task_set_task_data (task, g_strdup (runtime_id), g_free);

  /*
   * It would be tempting to just return early here if we could locate
   * the runtime as already registered. But that isn't enough since we
   * might need to also install an SDK.
   */

  peas_extension_set_foreach (self->extensions,
                              (PeasExtensionSetForeachFunc) install_lookup_cb,
                              &lookup);

  if (lookup.provider == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Failed to locate provider for runtime: %s",
                               runtime_id);
      IDE_EXIT;
    }

  ide_runtime_provider_install_async (lookup.provider,
                                      runtime_id,
                                      cancellable,
                                      ide_runtime_manager_install_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_runtime_manager_ensure_finish:
 * @self: an #IdeRuntimeManager
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous request to ide_runtime_manager_ensure_async()
 *
 * Returns: (transfer full): An #IdeRuntime or %NULL.
 */
IdeRuntime *
ide_runtime_manager_ensure_finish (IdeRuntimeManager  *self,
                                   GAsyncResult       *result,
                                   GError            **error)
{
  g_autoptr(GError) local_error = NULL;
  IdeRuntime *ret;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  ret = g_task_propagate_pointer (G_TASK (result), &local_error);

  /*
   * If we got NOT_SUPPORTED error, and the runtime already exists,
   * then we can synthesize a successful result to the caller.
   */
  if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    {
      const gchar *runtime_id = g_task_get_task_data (G_TASK (result));
      ret = ide_runtime_manager_get_runtime (self, runtime_id);
      if (ret != NULL)
        return ret;
    }

  if (error != NULL)
    *error = g_steal_pointer (&local_error);

  return ret;
}

static void
ide_runtime_manager_ensure_config_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeRuntimeProvider *provider = (IdeRuntimeProvider *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeRuntimeManager *self;
  const gchar *runtime_id;
  IdeRuntime *runtime;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_runtime_provider_bootstrap_finish (provider, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_RUNTIME_MANAGER (self));

  runtime_id = g_task_get_task_data (task);
  g_assert (runtime_id != NULL);

  runtime = ide_runtime_manager_get_runtime (self, runtime_id);

  if (runtime == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Runtime failed to register runtime \"%s\"",
                               runtime_id);
      IDE_EXIT;
    }

  g_task_return_pointer (task, g_object_ref (runtime), g_object_unref);

  IDE_EXIT;
}

/**
 * ide_runtime_manager_ensure_config_async:
 * @self: a #IdeRuntimeManager
 * @configuration: an #IdeConfiguration
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Ensures that the runtime or multiple runtimes that may be necessary to
 * build the configuration are installed.
 *
 * Since: 3.28
 */
void
ide_runtime_manager_ensure_config_async (IdeRuntimeManager   *self,
                                         IdeConfiguration    *configuration,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  const gchar *runtime_id;
  InstallLookup lookup = { 0 };

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (configuration));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  runtime_id = ide_configuration_get_runtime_id (configuration);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_runtime_manager_ensure_config_async);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_task_data (task, g_strdup (runtime_id), g_free);

  if (runtime_id == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Configuration does not have specified runtime");
      return;
    }

  /*
   * It would be tempting to just return early here if we could locate
   * the runtime as already registered. But that isn't enough since we
   * might need to also install an SDK.
   */

  lookup.runtime_id = runtime_id;
  peas_extension_set_foreach (self->extensions,
                              (PeasExtensionSetForeachFunc) install_lookup_cb,
                              &lookup);

  if (lookup.provider == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Failed to locate provider for runtime: %s",
                               runtime_id);
      IDE_EXIT;
    }

  ide_runtime_provider_bootstrap_async (lookup.provider,
                                        configuration,
                                        cancellable,
                                        ide_runtime_manager_ensure_config_cb,
                                        g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_runtime_manager_ensure_config_finish:
 * @self: a #IdeRuntimeManager
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous request to ide_runtime_manager_ensure_config_async().
 *
 * Returns: (transfer full): an #IdeRuntime
 *
 * Since: 3.28
 */
IdeRuntime *
ide_runtime_manager_ensure_config_finish (IdeRuntimeManager  *self,
                                          GAsyncResult       *result,
                                          GError            **error)
{
  g_autoptr(GError) local_error = NULL;
  IdeRuntime *ret;

  g_return_val_if_fail (IDE_IS_RUNTIME_MANAGER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  ret = g_task_propagate_pointer (G_TASK (result), &local_error);

  /*
   * If we got NOT_SUPPORTED error, and the runtime already exists,
   * then we can synthesize a successful result to the caller.
   */
  if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    {
      const gchar *runtime_id = g_task_get_task_data (G_TASK (result));
      ret = ide_runtime_manager_get_runtime (self, runtime_id);
      if (ret != NULL)
        return ret;
    }

  if (error != NULL)
    *error = g_steal_pointer (&local_error);

  return ret;
}
