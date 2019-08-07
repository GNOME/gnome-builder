/* ide-command-manager.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-command-manager"

#include "config.h"

#include <libpeas/peas.h>
#include <libide-plugins.h>
#include <libide-threading.h>

#include "ide-command.h"
#include "ide-command-manager.h"
#include "ide-command-provider.h"

struct _IdeCommandManager
{
  IdeObject               parent_instance;
  IdeExtensionSetAdapter *adapter;
};

typedef struct
{
  gchar        *typed_text;
  GPtrArray    *results;
  IdeWorkspace *workspace;
  gint          n_active;
} Query;

G_DEFINE_TYPE (IdeCommandManager, ide_command_manager, IDE_TYPE_OBJECT)

static void
query_free (Query *q)
{
  g_assert (q->n_active == 0);
  
  g_clear_object (&q->workspace);
  g_clear_pointer (&q->typed_text, g_free);
  g_clear_pointer (&q->results, g_ptr_array_unref);
  g_slice_free (Query, q);
}

static void
ide_command_manager_provider_added_cb (IdeExtensionSetAdapter *set,
                                       PeasPluginInfo         *plugin_info,
                                       PeasExtension          *exten,
                                       gpointer                user_data)
{
  IdeCommandProvider *provider = (IdeCommandProvider *)exten;
  IdeCommandManager *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_COMMAND_MANAGER (self));

  g_debug ("Adding command provider %s", G_OBJECT_TYPE_NAME (exten));

}

static void
ide_command_manager_provider_removed_cb (IdeExtensionSetAdapter *set,
                                         PeasPluginInfo         *plugin_info,
                                         PeasExtension          *exten,
                                         gpointer                user_data)
{
  IdeCommandProvider *provider = (IdeCommandProvider *)exten;
  IdeCommandManager *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_COMMAND_MANAGER (self));

  g_debug ("Removing command provider %s", G_OBJECT_TYPE_NAME (exten));

}

static void
ide_command_manager_parent_set (IdeObject *object,
                                IdeObject *parent)
{
  IdeCommandManager *self = (IdeCommandManager *)object;

  g_assert (IDE_IS_COMMAND_MANAGER (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  ide_clear_and_destroy_object (&self->adapter);

  if (parent == NULL)
    return;

  self->adapter = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                 peas_engine_get_default (),
                                                 IDE_TYPE_COMMAND_PROVIDER,
                                                 NULL, NULL);

  g_signal_connect (self->adapter,
                    "extension-added",
                    G_CALLBACK (ide_command_manager_provider_added_cb),
                    self);

  g_signal_connect (self->adapter,
                    "extension-removed",
                    G_CALLBACK (ide_command_manager_provider_removed_cb),
                    self);

  ide_extension_set_adapter_foreach (self->adapter,
                                     ide_command_manager_provider_added_cb,
                                     self);
}

static void
ide_command_manager_destroy (IdeObject *object)
{
  IdeCommandManager *self = (IdeCommandManager *)object;

  ide_clear_and_destroy_object (&self->adapter);

  IDE_OBJECT_CLASS (ide_command_manager_parent_class)->destroy (object);
}

static void
ide_command_manager_class_init (IdeCommandManagerClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_command_manager_destroy;
  i_object_class->parent_set = ide_command_manager_parent_set;
}

static void
ide_command_manager_init (IdeCommandManager *self)
{
}

/**
 * ide_command_manager_from_context:
 * @context: an #IdeContext
 *
 * Gets an #IdeCommandManager from the context.
 *
 * This may only be called on the main thread.
 *
 * Returns: (transfer none): an #IdeCommandManager
 *
 * Since: 3.34
 */
IdeCommandManager *
ide_command_manager_from_context (IdeContext *context)
{
  IdeCommandManager *self;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);

  if (!(self = ide_context_peek_child_typed (context, IDE_TYPE_COMMAND_MANAGER)))
    {
      g_autoptr(IdeCommandManager) freeme = NULL;
      self = freeme = ide_object_ensure_child_typed (IDE_OBJECT (context),
                                                     IDE_TYPE_COMMAND_MANAGER);
    }

  g_return_val_if_fail (IDE_IS_COMMAND_MANAGER (self), NULL);

  return self;
}

static void
ide_command_manager_query_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeCommandProvider *provider = (IdeCommandProvider *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GError) error = NULL;
  Query *q;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_COMMAND_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  q = ide_task_get_task_data (task);

  g_assert (q != NULL);
  g_assert (q->n_active > 0);

  if ((ret = ide_command_provider_query_finish (provider, result, &error)))
    g_ptr_array_extend_and_steal (q->results, g_steal_pointer (&ret));

  q->n_active--;

  if (q->n_active == 0)
    ide_task_return_pointer (task,
                             g_steal_pointer (&q->results),
                             g_ptr_array_unref);
}

static void
ide_command_manager_query_foreach_cb (IdeExtensionSetAdapter *set,
                                      PeasPluginInfo         *plugin_info,
                                      PeasExtension          *exten,
                                      gpointer                user_data)
{
  IdeCommandProvider *provider = (IdeCommandProvider *)exten;
  IdeTask *task = user_data;
  Query *q;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (task));

  q = ide_task_get_task_data (task);

  g_assert (q != NULL);
  g_assert (q->typed_text != NULL);
  g_assert (IDE_IS_WORKSPACE (q->workspace));

  q->n_active++;

  ide_command_provider_query_async (provider,
                                    q->workspace,
                                    q->typed_text,
                                    ide_task_get_cancellable (task),
                                    ide_command_manager_query_cb,
                                    g_object_ref (task));
}

void
ide_command_manager_query_async  (IdeCommandManager   *self,
                                  IdeWorkspace        *workspace,
                                  const gchar         *typed_text,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Query *q;

  g_return_if_fail (IDE_IS_COMMAND_MANAGER (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));
  g_return_if_fail (typed_text != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_command_manager_query_async);

  q = g_slice_new0 (Query);
  q->typed_text = g_strdup (typed_text);
  q->results = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_object_unref_and_destroy);
  q->workspace = g_object_ref (workspace);
  q->n_active = 0;
  ide_task_set_task_data (task, q, query_free);

  ide_extension_set_adapter_foreach (self->adapter,
                                     ide_command_manager_query_foreach_cb,
                                     task);

  if (q->n_active == 0)
    ide_task_return_pointer (task,
                             g_steal_pointer (&q->results),
                             g_ptr_array_unref);
}

/**
 * ide_command_manager_query_finish:
 *
 * Returns: (transfer full) (element-type IdeCommand): an array of
 *   #IdeCommand instances created by providers.
 *
 * Since: 3.34
 */
GPtrArray *
ide_command_manager_query_finish (IdeCommandManager  *self,
                                  GAsyncResult       *result,
                                  GError            **error)
{
  GPtrArray *ret;

  g_return_val_if_fail (IDE_IS_COMMAND_MANAGER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);
  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}
