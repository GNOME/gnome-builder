/* ide-toolchain-manager.c
 *
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
 *
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-toolchain-manager"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-threading.h>
#include <libpeas.h>

#include "ide-build-private.h"
#include "ide-pipeline.h"
#include "ide-config.h"
#include "ide-device.h"
#include "ide-simple-toolchain.h"
#include "ide-toolchain.h"
#include "ide-toolchain-manager.h"
#include "ide-toolchain-private.h"
#include "ide-toolchain-provider.h"

struct _IdeToolchainManager
{
  IdeObject         parent_instance;

  GCancellable     *cancellable;
  PeasExtensionSet *extensions;
  GPtrArray        *toolchains;
  guint             loaded : 1;
};

typedef struct
{
  IdePipeline *pipeline;
  gchar            *toolchain_id;
} PrepareState;

static void list_model_iface_init     (GListModelInterface *iface);
static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeToolchainManager, ide_toolchain_manager, IDE_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

static void
prepare_state_free (PrepareState *state)
{
  g_clear_object (&state->pipeline);
  g_clear_pointer (&state->toolchain_id, g_free);
  g_slice_free (PrepareState, state);
}

static void
ide_toolchain_manager_toolchain_added (IdeToolchainManager  *self,
                                       IdeToolchain         *toolchain,
                                       IdeToolchainProvider *provider)
{
  guint idx;

  IDE_ENTRY;

  g_assert (IDE_IS_TOOLCHAIN_MANAGER (self));
  g_assert (IDE_IS_TOOLCHAIN (toolchain));
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));

  idx = self->toolchains->len;
  g_ptr_array_add (self->toolchains, g_object_ref (toolchain));
  g_list_model_items_changed (G_LIST_MODEL (self), idx, 0, 1);

  IDE_EXIT;
}

static void
ide_toolchain_manager_toolchain_removed (IdeToolchainManager  *self,
                                         IdeToolchain         *toolchain,
                                         IdeToolchainProvider *provider)
{
  IDE_ENTRY;

  g_assert (IDE_IS_TOOLCHAIN_MANAGER (self));
  g_assert (IDE_IS_TOOLCHAIN (toolchain));
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));

  for (guint i = 0; i < self->toolchains->len; i++)
    {
      IdeToolchain *item = g_ptr_array_index (self->toolchains, i);

      if (toolchain == item)
        {
          g_ptr_array_remove_index (self->toolchains, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          break;
        }
    }

  IDE_EXIT;
}

static void
ide_toolchain_manager_toolchain_load_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeToolchainProvider *provider = (IdeToolchainProvider *)object;
  IdeContext *context;
  g_autoptr(IdeToolchainManager) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (self));
  g_assert (IDE_IS_TASK (result));

  context = ide_object_get_context (IDE_OBJECT (self));

  if (!ide_toolchain_provider_load_finish (provider, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        ide_context_warning (context,
                             "Failed to initialize toolchain provider: %s: %s",
                             G_OBJECT_TYPE_NAME (provider), error->message);
    }

  IDE_EXIT;
}

static void
provider_connect (IdeToolchainManager  *self,
                  IdeToolchainProvider *provider)
{
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (self));
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));

  g_signal_connect_object (provider,
                           "added",
                           G_CALLBACK (ide_toolchain_manager_toolchain_added),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (provider,
                           "removed",
                           G_CALLBACK (ide_toolchain_manager_toolchain_removed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
provider_disconnect (IdeToolchainManager  *self,
                     IdeToolchainProvider *provider)
{
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (self));
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_toolchain_manager_toolchain_added),
                                        self);
  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_toolchain_manager_toolchain_removed),
                                        self);
}

static void
ide_toolchain_manager_extension_added (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       GObject    *exten,
                                       gpointer          user_data)
{
  IdeToolchainManager *self = user_data;
  IdeToolchainProvider *provider = (IdeToolchainProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));

  provider_connect (self, provider);

  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (provider));

  ide_toolchain_provider_load_async (provider,
                                     self->cancellable,
                                     ide_toolchain_manager_toolchain_load_cb,
                                     g_object_ref (self));
}

static void
ide_toolchain_manager_extension_removed (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         GObject    *exten,
                                         gpointer          user_data)
{
  IdeToolchainManager *self = user_data;
  IdeToolchainProvider *provider = (IdeToolchainProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));

  provider_disconnect (self, provider);

  ide_toolchain_provider_unload (provider, self);

  ide_object_destroy (IDE_OBJECT (self));
}

static void
ide_toolchain_manager_init_load_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeToolchainProvider *provider = (IdeToolchainProvider *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  GPtrArray *providers;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_toolchain_provider_load_finish (provider, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        g_warning ("Failed to initialize toolchain provider: %s: %s",
                   G_OBJECT_TYPE_NAME (provider), error->message);
    }

  providers = ide_task_get_task_data (task);
  g_assert (providers != NULL);
  g_assert (providers->len > 0);

  if (!g_ptr_array_remove (providers, provider))
    g_critical ("Failed to locate provider in active set");

  if (providers->len == 0)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_toolchain_manager_collect_providers (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         GObject    *exten,
                                         gpointer          user_data)
{
  IdeToolchainProvider *provider = (IdeToolchainProvider *)exten;
  GPtrArray *providers = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));
  g_assert (providers != NULL);

  g_ptr_array_add (providers, g_object_ref (provider));
}

static void
ide_toolchain_manager_init_async (GAsyncInitable      *initable,
                                  gint                 priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  IdeToolchainManager *self = (IdeToolchainManager *)initable;
  g_autoptr(IdeSimpleToolchain) default_toolchain = NULL;
  g_autoptr(GPtrArray) providers = NULL;
  g_autoptr(IdeTask) task = NULL;
  guint idx;

  g_assert (G_IS_ASYNC_INITABLE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_toolchain_manager_init_async);
  ide_task_set_priority (task, priority);

#if 0
  g_signal_connect_swapped (task,
                            "notify::completed",
                            G_CALLBACK (notify_providers_loaded),
                            self);
#endif

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

  providers = g_ptr_array_new_with_free_func (g_object_unref);
  peas_extension_set_foreach (self->extensions,
                              ide_toolchain_manager_collect_providers,
                              providers);
  ide_task_set_task_data (task,
                          g_ptr_array_ref (providers),
                          g_ptr_array_unref);

  default_toolchain = ide_simple_toolchain_new ("default", _("Default (Host operating system)"));
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (default_toolchain));

  idx = self->toolchains->len;
  g_ptr_array_add (self->toolchains, g_steal_pointer (&default_toolchain));
  g_list_model_items_changed (G_LIST_MODEL (self), idx, 0, 1);

  for (guint i = 0; i < providers->len; i++)
    {
      IdeToolchainProvider *provider = g_ptr_array_index (providers, i);

      g_assert (IDE_IS_TOOLCHAIN_PROVIDER (provider));

      provider_connect (self, provider);

      ide_object_append (IDE_OBJECT (self), IDE_OBJECT (provider));

      ide_toolchain_provider_load_async (provider,
                                         cancellable,
                                         ide_toolchain_manager_init_load_cb,
                                         g_object_ref (task));
    }

  if (providers->len == 0)
    ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_toolchain_manager_init_finish (GAsyncInitable  *initable,
                                   GAsyncResult    *result,
                                   GError         **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (initable));
  g_assert (IDE_IS_TASK (result));

  IDE_TOOLCHAIN_MANAGER (initable)->loaded = TRUE;

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_toolchain_manager_init_async;
  iface->init_finish = ide_toolchain_manager_init_finish;
}

static void
ide_toolchain_manager_destroy (IdeObject *object)
{
  IdeToolchainManager *self = (IdeToolchainManager *)object;

  g_clear_object (&self->extensions);
  g_clear_pointer (&self->toolchains, g_ptr_array_unref);
  g_clear_object (&self->cancellable);

  IDE_OBJECT_CLASS (ide_toolchain_manager_parent_class)->destroy (object);
}

static void
ide_toolchain_manager_class_init (IdeToolchainManagerClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_toolchain_manager_destroy;
}

static void
ide_toolchain_manager_init (IdeToolchainManager *self)
{
  self->loaded = FALSE;
  self->cancellable = g_cancellable_new ();
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

/**
 * ide_toolchain_manager_get_toolchain:
 * @self: An #IdeToolchainManager
 * @id: the identifier of the toolchain
 *
 * Gets the toolchain by its internal identifier.
 *
 * Returns: (transfer full): An #IdeToolchain.
 */
IdeToolchain *
ide_toolchain_manager_get_toolchain (IdeToolchainManager *self,
                                     const gchar         *id)
{
  g_return_val_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  for (guint i = 0; i < self->toolchains->len; i++)
    {
      IdeToolchain *toolchain = g_ptr_array_index (self->toolchains, i);
      const gchar *toolchain_id = ide_toolchain_get_id (toolchain);

      if (g_strcmp0 (toolchain_id, id) == 0)
        return g_object_ref (toolchain);
    }

  return NULL;
}

/**
 * ide_toolchain_manager_is_loaded:
 * @self: An #IdeToolchainManager
 *
 * Gets whether all the #IdeToolchainProvider implementations are loaded
 * and have registered all their #IdeToolchain.
 *
 * Returns: %TRUE if all the toolchains are loaded
 */
gboolean
ide_toolchain_manager_is_loaded (IdeToolchainManager  *self)
{
  g_return_val_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self), FALSE);

  return self->loaded;
}

void
_ide_toolchain_manager_prepare_async (IdeToolchainManager  *self,
                                      IdePipeline     *pipeline,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(IdeToolchain) toolchain = NULL;
  IdeConfig *config;
  PrepareState *state;
  const gchar *toolchain_id;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TOOLCHAIN_MANAGER (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  config = ide_pipeline_get_config (pipeline);
  toolchain_id = ide_config_get_toolchain_id (config);

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

  g_task_return_pointer (task, g_object_ref (toolchain), g_object_unref);

  IDE_EXIT;
}

gboolean
_ide_toolchain_manager_prepare_finish (IdeToolchainManager  *self,
                                       GAsyncResult         *result,
                                       GError              **error)
{
  g_autoptr(GError) local_error = NULL;
  g_autoptr(IdeToolchain) ret = NULL;
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
    _ide_pipeline_set_toolchain (state->pipeline, ret);

  IDE_RETURN (ret != NULL);
}
