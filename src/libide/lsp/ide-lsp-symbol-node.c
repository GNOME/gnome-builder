/* ide-lsp-symbol-node.c
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

#define G_LOG_DOMAIN "ide-lsp-symbol-node"

#include "config.h"

#include <libide-code.h>
#include <libide-threading.h>

#include "ide-lsp-symbol-node.h"
#include "ide-lsp-symbol-node-private.h"
#include "ide-lsp-util.h"

typedef struct
{
  guint line;
  guint column;
} Location;

typedef struct
{
  GFile *file;
  gchar *parent_name;
  IdeSymbolKind kind;
  Location begin;
  Location end;
} IdeLspSymbolNodePrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (IdeLspSymbolNode, ide_lsp_symbol_node, IDE_TYPE_SYMBOL_NODE)

static inline gint
location_compare (const Location *a,
                  const Location *b)
{
  gint ret;

  ret = (gint)a->line - (gint)b->line;
  if (ret == 0)
    ret = (gint)a->column - (gint)b->column;

  return ret;
}

static void
ide_lsp_symbol_node_get_location_async (IdeSymbolNode       *node,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeLspSymbolNode *self = (IdeLspSymbolNode *)node;
  IdeLspSymbolNodePrivate *priv = ide_lsp_symbol_node_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SYMBOL_NODE (node));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_symbol_node_get_location_async);
  ide_task_return_pointer (task,
                           ide_location_new (priv->file, priv->begin.line, priv->begin.column),
                           g_object_unref);

  IDE_EXIT;
}

static IdeLocation *
ide_lsp_symbol_node_get_location_finish (IdeSymbolNode  *node,
                                         GAsyncResult   *result,
                                         GError        **error)
{
  IdeLocation *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SYMBOL_NODE (node));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_lsp_symbol_node_finalize (GObject *object)
{
  IdeLspSymbolNode *self = (IdeLspSymbolNode *)object;
  IdeLspSymbolNodePrivate *priv = ide_lsp_symbol_node_get_instance_private (self);

  g_clear_pointer (&priv->parent_name, g_free);
  g_clear_object (&priv->file);

  G_OBJECT_CLASS (ide_lsp_symbol_node_parent_class)->finalize (object);
}

static void
ide_lsp_symbol_node_class_init (IdeLspSymbolNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSymbolNodeClass *symbol_node_class = IDE_SYMBOL_NODE_CLASS (klass);

  object_class->finalize = ide_lsp_symbol_node_finalize;

  symbol_node_class->get_location_async = ide_lsp_symbol_node_get_location_async;
  symbol_node_class->get_location_finish = ide_lsp_symbol_node_get_location_finish;
}

static void
ide_lsp_symbol_node_init (IdeLspSymbolNode *self)
{
  self->gnode.data = self;
}

IdeLspSymbolNode *
ide_lsp_symbol_node_new (GFile       *file,
                         const gchar *name,
                         const gchar *parent_name,
                         gint         kind,
                         guint        begin_line,
                         guint        begin_column,
                         guint        end_line,
                         guint        end_column,
                         gboolean     deprecated)
{
  IdeLspSymbolNode *self;
  IdeLspSymbolNodePrivate *priv;
  IdeSymbolFlags flags = 0;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  kind = ide_lsp_decode_symbol_kind (kind);

  if (deprecated)
    flags |= IDE_SYMBOL_FLAGS_IS_DEPRECATED;

  self = g_object_new (IDE_TYPE_LSP_SYMBOL_NODE,
                       "flags", flags,
                       "kind", kind,
                       "name", name,
                       NULL);
  priv = ide_lsp_symbol_node_get_instance_private (self);

  priv->file = g_object_ref (file);
  priv->parent_name = g_strdup (parent_name);
  priv->begin.line = begin_line;
  priv->begin.column = begin_column;
  priv->end.line = end_line;
  priv->end.column = end_column;

  return self;
}

const gchar *
ide_lsp_symbol_node_get_parent_name (IdeLspSymbolNode *self)
{
  IdeLspSymbolNodePrivate *priv = ide_lsp_symbol_node_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_SYMBOL_NODE (self), NULL);

  return priv->parent_name;
}

gboolean
ide_lsp_symbol_node_is_parent_of (IdeLspSymbolNode *self,
                                  IdeLspSymbolNode *other)
{
  IdeLspSymbolNodePrivate *priv = ide_lsp_symbol_node_get_instance_private (self);
  IdeLspSymbolNodePrivate *opriv = ide_lsp_symbol_node_get_instance_private (other);

  g_return_val_if_fail (IDE_IS_LSP_SYMBOL_NODE (self), FALSE);
  g_return_val_if_fail (IDE_IS_LSP_SYMBOL_NODE (other), FALSE);

  return (location_compare (&priv->begin, &opriv->begin) <= 0) &&
         (location_compare (&priv->end, &opriv->end) >= 0);
}
