/* gbp-command-bar-model.c
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

#define G_LOG_DOMAIN "gbp-command-bar-model"

#include "config.h"

#include <libide-gui.h>
#include <libide-threading.h>
#include <libpeas/peas.h>
#include <libpeas/peas-autocleanups.h>

#include "gbp-command-bar-suggestion.h"
#include "gbp-command-bar-model.h"

struct _GbpCommandBarModel
{
  IdeObject  parent_instance;
  GPtrArray *items;
};

typedef struct
{
  IdeWorkspace *workspace;
  IdeTask      *task;
  const gchar  *typed_text;
  GPtrArray    *providers;
} Complete;

static GType
gbp_command_bar_model_get_item_type (GListModel *model)
{
  return GBP_TYPE_COMMAND_BAR_SUGGESTION;
}

static guint
gbp_command_bar_model_get_n_items (GListModel *model)
{
  return GBP_COMMAND_BAR_MODEL (model)->items->len;
}

static gpointer
gbp_command_bar_model_get_item (GListModel *model,
                                guint       position)
{
  GbpCommandBarModel *self = (GbpCommandBarModel *)model;

  g_assert (GBP_IS_COMMAND_BAR_MODEL (self));

  if (position < self->items->len)
    return g_object_ref (g_ptr_array_index (self->items, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gbp_command_bar_model_get_item_type;
  iface->get_n_items = gbp_command_bar_model_get_n_items;
  iface->get_item = gbp_command_bar_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GbpCommandBarModel, gbp_command_bar_model, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
gbp_command_bar_model_dispose (GObject *object)
{
  GbpCommandBarModel *self = (GbpCommandBarModel *)object;

  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_command_bar_model_parent_class)->dispose (object);
}

static void
gbp_command_bar_model_class_init (GbpCommandBarModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_command_bar_model_dispose;
}

static void
gbp_command_bar_model_init (GbpCommandBarModel *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

GbpCommandBarModel *
gbp_command_bar_model_new (IdeContext *context)
{
  GbpCommandBarModel *self;

  self = g_object_new (GBP_TYPE_COMMAND_BAR_MODEL, NULL);
  ide_object_append (IDE_OBJECT (context), IDE_OBJECT (self));

  return g_steal_pointer (&self);
}

static void
gbp_command_bar_model_query_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeCommandProvider *provider = (IdeCommandProvider *)object;
  g_autoptr(GPtrArray) items = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpCommandBarModel *self;
  GPtrArray *providers;
  guint position;

  g_assert (IDE_IS_COMMAND_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  providers = ide_task_get_task_data (task);

  g_assert (GBP_IS_COMMAND_BAR_MODEL (self));
  g_assert (providers != NULL);

  position = self->items->len;

  if ((items = ide_command_provider_query_finish (provider, result, &error)))
    {
      for (guint i = 0; i < items->len; i++)
        {
          IdeCommand *command = g_ptr_array_index (items, i);

          g_ptr_array_add (self->items, gbp_command_bar_suggestion_new (command));
        }

      g_list_model_items_changed (G_LIST_MODEL (self), position, 0, items->len);
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (items, g_object_unref);

  g_ptr_array_remove (providers, provider);

  if (providers->len == 0)
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_command_bar_query_foreach_cb (PeasExtensionSet *set,
                                  PeasPluginInfo   *plugin_info,
                                  PeasExtension    *exten,
                                  gpointer          user_data)
{
  IdeCommandProvider *provider = (IdeCommandProvider *)exten;
  Complete *complete = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_COMMAND_PROVIDER (provider));
  g_assert (complete != NULL);

  g_ptr_array_add (complete->providers, g_object_ref (provider));

  ide_command_provider_query_async (provider,
                                    complete->workspace,
                                    complete->typed_text,
                                    ide_task_get_cancellable (complete->task),
                                    gbp_command_bar_model_query_cb,
                                    g_object_ref (complete->task));
}

void
gbp_command_bar_model_complete_async (GbpCommandBarModel  *self,
                                      IdeWorkspace        *workspace,
                                      const gchar         *typed_text,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(PeasExtensionSet) set = NULL;
  g_autoptr(GPtrArray) providers = NULL;
  g_autoptr(IdeTask) task = NULL;
  Complete complete = {0};

  g_return_if_fail (GBP_IS_COMMAND_BAR_MODEL (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));
  g_return_if_fail (typed_text != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  providers = g_ptr_array_new_with_free_func (g_object_unref);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_command_bar_model_complete_async);
  ide_task_set_task_data (task, g_ptr_array_ref (providers), g_ptr_array_unref);

  set = peas_extension_set_new (peas_engine_get_default (),
                                IDE_TYPE_COMMAND_PROVIDER,
                                NULL);

  complete.workspace = workspace;
  complete.providers = providers;
  complete.typed_text = typed_text;
  complete.task = task;

  peas_extension_set_foreach (set, gbp_command_bar_query_foreach_cb, &complete);

  if (providers->len == 0)
    ide_task_return_boolean (task, TRUE);
}

gboolean
gbp_command_bar_model_complete_finish (GbpCommandBarModel  *self,
                                       GAsyncResult        *result,
                                       GError             **error)
{
  g_return_val_if_fail (GBP_IS_COMMAND_BAR_MODEL (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
