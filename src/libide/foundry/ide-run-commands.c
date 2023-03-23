/* ide-run-commands.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-run-commands"

#include "config.h"

#include <gtk/gtk.h>

#include <libide-plugins.h>
#include <libide-threading.h>

#include "ide-run-command.h"
#include "ide-run-command-provider.h"
#include "ide-run-commands.h"

#define RELOAD_TIMEOUT_MSEC 250

struct _IdeRunCommands
{
  IdeObject               parent_instance;
  IdeExtensionSetAdapter *addins;
  GListStore             *models;
  GtkFlattenListModel    *flatten_model;
  GHashTable             *provider_to_model;
  GQueue                  invalid;
  guint                   reload_source;
};

static GType
ide_run_commands_get_item_type (GListModel *model)
{
  return IDE_TYPE_RUN_COMMAND;
}

static guint
ide_run_commands_get_n_items (GListModel *model)
{
  IdeRunCommands *self = IDE_RUN_COMMANDS (model);
  return g_list_model_get_n_items (G_LIST_MODEL (self->flatten_model));
}

static gpointer
ide_run_commands_get_item (GListModel *model,
                           guint       position)
{
  IdeRunCommands *self = IDE_RUN_COMMANDS (model);
  return g_list_model_get_item (G_LIST_MODEL (self->flatten_model), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_run_commands_get_item_type;
  iface->get_n_items = ide_run_commands_get_n_items;
  iface->get_item = ide_run_commands_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeRunCommands, ide_run_commands, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ide_run_commands_items_changed_cb (IdeRunCommands *self,
                                   guint           position,
                                   guint           removed,
                                   guint           added,
                                   GListModel     *model)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_COMMANDS (self));
  g_assert (G_IS_LIST_MODEL (model));

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);

  IDE_EXIT;
}

static void
ide_run_commands_list_commands_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeRunCommandProvider *provider = (IdeRunCommandProvider *)object;
  g_autoptr(IdeRunCommands) self = user_data;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GError) error = NULL;
  GListModel *old_model;
  gboolean found;
  guint position;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_COMMAND_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_RUN_COMMANDS (self));

  if (!(model = ide_run_command_provider_list_commands_finish (provider, result, &error)))
    {
      /* Just keep the old one around until things succeed */
      if (!ide_error_ignore (error))
        g_debug ("Failed to list run commands from %s: %s",
                 G_OBJECT_TYPE_NAME (provider), error->message);
      IDE_EXIT;
    }

  /* Do nothing if the model didn't change */
  old_model = g_hash_table_lookup (self->provider_to_model, provider);
  if (old_model == model)
    IDE_EXIT;

  g_assert (old_model != model);
  g_assert (!old_model || G_IS_LIST_MODEL (old_model));
  g_assert (G_IS_LIST_MODEL (model));

  /* First try to locate our model */
  if (old_model != NULL)
    found = g_list_store_find (self->models, old_model, &position);
  else
    found = FALSE;

  /* Now ensure our hashtable is up to date for re-entrancy purposes */
  g_hash_table_insert (self->provider_to_model,
                       g_object_ref (provider),
                       g_object_ref (model));

  if (found)
    g_list_store_splice (self->models, position, 1, (gpointer *)&model, 1);
  else
    g_list_store_append (self->models, model);

  IDE_EXIT;
}

static gboolean
ide_run_commands_reload_source_func (gpointer data)
{
  g_autoptr(GCancellable) cancellable = NULL;
  IdeRunCommandProvider *provider;
  IdeRunCommands *self = data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_COMMANDS (self));

  self->reload_source = 0;

  cancellable = ide_object_ref_cancellable (IDE_OBJECT (self));

  while ((provider = g_queue_pop_head (&self->invalid)))
    ide_run_command_provider_list_commands_async (provider,
                                                  cancellable,
                                                  ide_run_commands_list_commands_cb,
                                                  g_object_ref (self));

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_run_commands_provider_invalidated_cb (IdeRunCommands        *self,
                                          IdeRunCommandProvider *provider)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_COMMANDS (self));
  g_assert (IDE_IS_RUN_COMMAND_PROVIDER (provider));

  if (g_queue_find (&self->invalid, provider) == NULL)
    {
      g_queue_push_tail (&self->invalid, provider);

      if (self->reload_source == 0)
        self->reload_source = g_timeout_add (RELOAD_TIMEOUT_MSEC,
                                             ide_run_commands_reload_source_func,
                                             self);
    }

  IDE_EXIT;
}

static void
ide_run_commands_provider_added_cb (IdeExtensionSetAdapter *set,
                                    PeasPluginInfo         *plugin_info,
                                    GObject          *exten,
                                    gpointer                user_data)
{
  IdeRunCommandProvider *provider = (IdeRunCommandProvider *)exten;
  IdeRunCommands *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_RUN_COMMANDS (self));

  g_signal_connect_object (provider,
                           "invalidated",
                           G_CALLBACK (ide_run_commands_provider_invalidated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  ide_run_commands_provider_invalidated_cb (self, provider);

  IDE_EXIT;
}

static void
ide_run_commands_provider_removed_cb (IdeExtensionSetAdapter *set,
                                      PeasPluginInfo         *plugin_info,
                                      GObject          *exten,
                                      gpointer                user_data)
{
  IdeRunCommandProvider *provider = (IdeRunCommandProvider *)exten;
  g_autoptr(IdeRunCommandProvider) stolen_key = NULL;
  g_autoptr(GListModel) stolen_value = NULL;
  IdeRunCommands *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_RUN_COMMANDS (self));

  if (g_hash_table_steal_extended (self->provider_to_model,
                                   provider,
                                   (gpointer *)&stolen_key,
                                   (gpointer *)&stolen_value))
    {
      g_queue_remove (&self->invalid, provider);

      if (self->invalid.length == 0)
        g_clear_handle_id (&self->reload_source, g_source_remove);

      if (stolen_value != NULL)
        {
          guint position;

          if (g_list_store_find (self->models, stolen_value, &position))
            g_list_store_remove (self->models, position);
        }
    }

  IDE_EXIT;
}

static void
ide_run_commands_parent_set (IdeObject *object,
                             IdeObject *parent)
{
  IdeRunCommands *self = (IdeRunCommands *)object;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_COMMANDS (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    IDE_EXIT;

  self->addins = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                peas_engine_get_default (),
                                                IDE_TYPE_RUN_COMMAND_PROVIDER,
                                                NULL, NULL);
  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_run_commands_provider_added_cb),
                    self);
  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_run_commands_provider_removed_cb),
                    self);
  ide_extension_set_adapter_foreach (self->addins,
                                     ide_run_commands_provider_added_cb,
                                     self);

  IDE_EXIT;
}

static void
ide_run_commands_destroy (IdeObject *object)
{
  IdeRunCommands *self = (IdeRunCommands *)object;

  g_queue_clear (&self->invalid);
  g_clear_handle_id (&self->reload_source, g_source_remove);
  ide_clear_and_destroy_object (&self->addins);
  g_clear_object (&self->models);
  g_clear_object (&self->flatten_model);
  g_clear_pointer (&self->provider_to_model, g_hash_table_unref);

  IDE_OBJECT_CLASS (ide_run_commands_parent_class)->destroy (object);
}

static void
ide_run_commands_class_init (IdeRunCommandsClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_run_commands_destroy;
  i_object_class->parent_set = ide_run_commands_parent_set;
}

static void
ide_run_commands_init (IdeRunCommands *self)
{
  self->provider_to_model = g_hash_table_new_full (NULL, NULL, g_object_unref, g_object_unref);
  self->models = g_list_store_new (G_TYPE_LIST_MODEL);
  self->flatten_model = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->models)));

  g_signal_connect_object (self->flatten_model,
                           "items-changed",
                           G_CALLBACK (ide_run_commands_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

/**
 * ide_run_commands_dup_by_id:
 * @self: a #IdeRunCommands
 * @id: (nullable): the id of the run command
 *
 * Finds an #IdeRunCommand by it's id.
 *
 * %NULL is allowed for @id out of convenience, but will return %NULL.
 *
 * Returns: (transfer full) (nullable): an #IdeRunCommand or %NULL
 */
IdeRunCommand *
ide_run_commands_dup_by_id (IdeRunCommands *self,
                            const char     *id)
{
  guint n_items;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RUN_COMMANDS (self), NULL);

  if (id == NULL)
    IDE_RETURN (NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));

  IDE_TRACE_MSG ("Locating command by id %s in list of %u commands", id, n_items);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeRunCommand) run_command = g_list_model_get_item (G_LIST_MODEL (self), i);
      const char *run_command_id = ide_run_command_get_id (run_command);

      if (ide_str_equal0 (run_command_id, id))
        IDE_RETURN (g_steal_pointer (&run_command));
    }

  IDE_RETURN (NULL);
}

static gboolean
filter_run_command_by_kind (gpointer item,
                            gpointer user_data)
{
  return ide_run_command_get_kind (item) == GPOINTER_TO_INT (user_data);
}

/**
 * ide_run_commands_list_by_kind:
 * @self: an #IdeRunCommands
 * @kind: an #IdeRunCommandKind
 *
 * Creates a new #GListModel of #IdeRunCommand filtered by @kind
 *
 * The model will update as new commands are added or removed from @self.
 *
 * Returns: (transfer full): a #GListModel
 */
GListModel *
ide_run_commands_list_by_kind (IdeRunCommands    *self,
                               IdeRunCommandKind  kind)
{
  GtkCustomFilter *filter = NULL;
  GtkFilterListModel *model = NULL;

  g_return_val_if_fail (IDE_IS_RUN_COMMANDS (self), NULL);

  filter = gtk_custom_filter_new (filter_run_command_by_kind, GINT_TO_POINTER (kind), NULL);
  model = gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (self)), GTK_FILTER (filter));

  return G_LIST_MODEL (model);
}
