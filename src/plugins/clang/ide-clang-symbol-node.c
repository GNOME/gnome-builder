/* ide-clang-symbol-node.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-clang-symbol-node"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "ide-clang-symbol-node.h"

struct _IdeClangSymbolNode
{
  IdeSymbolNode  parent_instance;
  IdeSymbol     *symbol;
  GVariant      *children;
};

G_DEFINE_FINAL_TYPE (IdeClangSymbolNode, ide_clang_symbol_node, IDE_TYPE_SYMBOL_NODE)

IdeSymbolNode *
ide_clang_symbol_node_new (GVariant *node)
{
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GVariant) children = NULL;
  IdeClangSymbolNode *self;
  const gchar *name;

  g_return_val_if_fail (node != NULL, NULL);

  if (!(symbol = ide_symbol_new_from_variant (node)))
    return NULL;

  name = ide_symbol_get_name (symbol);

  self = g_object_new (IDE_TYPE_CLANG_SYMBOL_NODE,
                       "kind", ide_symbol_get_kind (symbol),
                       "flags", ide_symbol_get_flags (symbol),
                       "name", ide_str_empty0 (name) ? _("anonymous") : name,
                       NULL);

  self->symbol = g_steal_pointer (&symbol);

  if ((children = g_variant_lookup_value (node, "children", NULL)))
    {
      if (g_variant_is_of_type (children, G_VARIANT_TYPE_VARIANT))
        self->children = g_variant_get_variant (children);
      else if (g_variant_is_of_type (children, G_VARIANT_TYPE ("aa{sv}")) ||
               g_variant_is_of_type (children, G_VARIANT_TYPE ("av")))
        self->children = g_steal_pointer (&children);
    }

  return IDE_SYMBOL_NODE (self);
}

static void
ide_clang_symbol_node_get_location_async (IdeSymbolNode       *symbol_node,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  IdeClangSymbolNode *self = (IdeClangSymbolNode *)symbol_node;
  g_autoptr(IdeTask) task = NULL;
  IdeLocation *location;

  g_return_if_fail (IDE_IS_CLANG_SYMBOL_NODE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_symbol_node_get_location_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  if (self->symbol == NULL ||
      (!(location = ide_symbol_get_location (self->symbol)) &&
       !(location = ide_symbol_get_header_location (self->symbol))))
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Failed to locate location for symbol");
  else
    ide_task_return_pointer (task,
                             g_object_ref (location),
                             g_object_unref);
}

static IdeLocation *
ide_clang_symbol_node_get_location_finish (IdeSymbolNode  *symbol_node,
                                           GAsyncResult   *result,
                                           GError        **error)
{
  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_NODE (symbol_node), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_clang_symbol_node_finalize (GObject *object)
{
  IdeClangSymbolNode *self = (IdeClangSymbolNode *)object;

  g_clear_object (&self->symbol);
  g_clear_pointer (&self->children, g_variant_unref);

  G_OBJECT_CLASS (ide_clang_symbol_node_parent_class)->finalize (object);
}

static void
ide_clang_symbol_node_class_init (IdeClangSymbolNodeClass *klass)
{
  IdeSymbolNodeClass *node_class = IDE_SYMBOL_NODE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_symbol_node_finalize;

  node_class->get_location_async = ide_clang_symbol_node_get_location_async;
  node_class->get_location_finish = ide_clang_symbol_node_get_location_finish;
}

static void
ide_clang_symbol_node_init (IdeClangSymbolNode *self)
{
}

guint
ide_clang_symbol_node_get_n_children (IdeClangSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_NODE (self), 0);

  return self->children ? g_variant_n_children (self->children) : 0;
}

IdeSymbolNode *
ide_clang_symbol_node_get_nth_child (IdeClangSymbolNode *self,
                                     guint               nth)
{
  g_autoptr(GVariant) child = NULL;

  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_NODE (self), NULL);

  if (self->children == NULL || g_variant_n_children (self->children) <= nth)
    return NULL;

  child = g_variant_get_child_value (self->children, nth);

  return ide_clang_symbol_node_new (child);
}
