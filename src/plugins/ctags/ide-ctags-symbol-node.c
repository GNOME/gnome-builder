/* ide-ctags-symbol-node.c
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

#define G_LOG_DOMAIN "ide-ctags-symbol-node"

#include "ide-ctags-symbol-node.h"

struct _IdeCtagsSymbolNode
{
  IdeSymbolNode             parent_instance;
  IdeCtagsIndex            *index;
  IdeCtagsSymbolResolver   *resolver;
  const IdeCtagsIndexEntry *entry;
  GPtrArray                *children;
};

G_DEFINE_FINAL_TYPE (IdeCtagsSymbolNode, ide_ctags_symbol_node, IDE_TYPE_SYMBOL_NODE)

static void
ide_ctags_symbol_node_get_location_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeCtagsSymbolResolver *resolver = (IdeCtagsSymbolResolver *)object;
  g_autoptr(IdeLocation) location = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  location = ide_ctags_symbol_resolver_get_location_finish (resolver, result, &error);

  if (location == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&location),
                             (GDestroyNotify)g_object_unref);
}

static void
ide_ctags_symbol_node_get_location_async (IdeSymbolNode       *node,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  IdeCtagsSymbolNode *self = (IdeCtagsSymbolNode *)node;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_CTAGS_SYMBOL_NODE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_ctags_symbol_node_get_location_async);

  ide_ctags_symbol_resolver_get_location_async (self->resolver,
                                                self->index,
                                                self->entry,
                                                NULL,
                                                ide_ctags_symbol_node_get_location_cb,
                                                g_steal_pointer (&task));
}

static IdeLocation *
ide_ctags_symbol_node_get_location_finish (IdeSymbolNode  *node,
                                           GAsyncResult   *result,
                                           GError        **error)
{
  g_return_val_if_fail (IDE_IS_CTAGS_SYMBOL_NODE (node), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_ctags_symbol_node_finalize (GObject *object)
{
  IdeCtagsSymbolNode *self = (IdeCtagsSymbolNode *)object;

  g_clear_pointer (&self->children, g_ptr_array_unref);
  self->entry = NULL;
  g_clear_object (&self->index);

  G_OBJECT_CLASS (ide_ctags_symbol_node_parent_class)->finalize (object);
}

static void
ide_ctags_symbol_node_class_init (IdeCtagsSymbolNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSymbolNodeClass *symbol_node_class = IDE_SYMBOL_NODE_CLASS (klass);

  object_class->finalize = ide_ctags_symbol_node_finalize;

  symbol_node_class->get_location_async = ide_ctags_symbol_node_get_location_async;
  symbol_node_class->get_location_finish = ide_ctags_symbol_node_get_location_finish;
}

static void
ide_ctags_symbol_node_init (IdeCtagsSymbolNode *self)
{
}

IdeCtagsSymbolNode *
ide_ctags_symbol_node_new (IdeCtagsSymbolResolver   *resolver,
                           IdeCtagsIndex            *index,
                           const IdeCtagsIndexEntry *entry)
{
  IdeCtagsSymbolNode *self;
  IdeSymbolFlags flags = IDE_SYMBOL_FLAGS_NONE;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (resolver));
  g_assert (IDE_IS_CTAGS_INDEX (index));
  g_assert (entry != NULL);

  self = g_object_new (IDE_TYPE_CTAGS_SYMBOL_NODE,
                       "name", entry->name,
                       "kind", ide_ctags_index_entry_kind_to_symbol_kind (entry->kind),
                       "flags", flags,
                       NULL);

  self->entry = entry;
  self->index = g_object_ref (index);
  self->resolver = g_object_ref (resolver);

  return self;
}

guint
ide_ctags_symbol_node_get_n_children (IdeCtagsSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_CTAGS_SYMBOL_NODE (self), 0);

  return self->children != NULL ? self->children->len : 0;
}

IdeSymbolNode *
ide_ctags_symbol_node_get_nth_child (IdeCtagsSymbolNode *self,
                                     guint               nth_child)
{
  g_return_val_if_fail (IDE_IS_CTAGS_SYMBOL_NODE (self), NULL);

  if (self->children != NULL && nth_child < self->children->len)
    return g_object_ref (g_ptr_array_index (self->children, nth_child));

  return NULL;
}

void
ide_ctags_symbol_node_take_child (IdeCtagsSymbolNode *self,
                                  IdeCtagsSymbolNode *child)
{
  g_return_if_fail (IDE_IS_CTAGS_SYMBOL_NODE (self));
  g_return_if_fail (IDE_IS_CTAGS_SYMBOL_NODE (child));

  if (self->children == NULL)
    self->children = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (self->children, child);
}

const IdeCtagsIndexEntry *
ide_ctags_symbol_node_get_entry (IdeCtagsSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_CTAGS_SYMBOL_NODE (self), NULL);

  return self->entry;
}
