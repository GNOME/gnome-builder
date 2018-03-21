/* ide-langserv-symbol-node.c
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

#define G_LOG_DOMAIN "ide-langserv-symbol-node"

#include "ide-debug.h"

#include "diagnostics/ide-source-location.h"
#include "files/ide-file.h"
#include "langserv/ide-langserv-symbol-node.h"
#include "langserv/ide-langserv-symbol-node-private.h"
#include "langserv/ide-langserv-util.h"
#include "threading/ide-task.h"

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
} IdeLangservSymbolNodePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLangservSymbolNode, ide_langserv_symbol_node, IDE_TYPE_SYMBOL_NODE)

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
ide_langserv_symbol_node_get_location_async (IdeSymbolNode       *node,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  IdeLangservSymbolNode *self = (IdeLangservSymbolNode *)node;
  IdeLangservSymbolNodePrivate *priv = ide_langserv_symbol_node_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeFile) ifile = NULL;
  g_autoptr(IdeSourceLocation) location = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_SYMBOL_NODE (node));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_langserv_symbol_node_get_location_async);

  ifile = ide_file_new (NULL, priv->file);
  location = ide_source_location_new (ifile, priv->begin.line, priv->begin.column, 0);

  ide_task_return_pointer (task, g_steal_pointer (&location), (GDestroyNotify)ide_source_location_unref);

  IDE_EXIT;
}

static IdeSourceLocation *
ide_langserv_symbol_node_get_location_finish (IdeSymbolNode  *node,
                                              GAsyncResult   *result,
                                              GError        **error)
{
  IdeSourceLocation *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_SYMBOL_NODE (node));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_langserv_symbol_node_finalize (GObject *object)
{
  IdeLangservSymbolNode *self = (IdeLangservSymbolNode *)object;
  IdeLangservSymbolNodePrivate *priv = ide_langserv_symbol_node_get_instance_private (self);

  g_clear_pointer (&priv->parent_name, g_free);
  g_clear_object (&priv->file);

  G_OBJECT_CLASS (ide_langserv_symbol_node_parent_class)->finalize (object);
}

static void
ide_langserv_symbol_node_class_init (IdeLangservSymbolNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSymbolNodeClass *symbol_node_class = IDE_SYMBOL_NODE_CLASS (klass);

  object_class->finalize = ide_langserv_symbol_node_finalize;

  symbol_node_class->get_location_async = ide_langserv_symbol_node_get_location_async;
  symbol_node_class->get_location_finish = ide_langserv_symbol_node_get_location_finish;
}

static void
ide_langserv_symbol_node_init (IdeLangservSymbolNode *self)
{
  self->gnode.data = self;
}

IdeLangservSymbolNode *
ide_langserv_symbol_node_new (GFile       *file,
                              const gchar *name,
                              const gchar *parent_name,
                              gint         kind,
                              guint        begin_line,
                              guint        begin_column,
                              guint        end_line,
                              guint        end_column)
{
  IdeLangservSymbolNode *self;
  IdeLangservSymbolNodePrivate *priv;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  kind = ide_langserv_decode_symbol_kind (kind);

  self = g_object_new (IDE_TYPE_LANGSERV_SYMBOL_NODE,
                       "flags", 0,
                       "kind", kind,
                       "name", name,
                       NULL);
  priv = ide_langserv_symbol_node_get_instance_private (self);

  priv->file = g_object_ref (file);
  priv->parent_name = g_strdup (parent_name);
  priv->begin.line = begin_line;
  priv->begin.column = begin_column;
  priv->end.line = end_line;
  priv->end.column = end_column;

  return self;
}

const gchar *
ide_langserv_symbol_node_get_parent_name (IdeLangservSymbolNode *self)
{
  IdeLangservSymbolNodePrivate *priv = ide_langserv_symbol_node_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LANGSERV_SYMBOL_NODE (self), NULL);

  return priv->parent_name;
}

gboolean
ide_langserv_symbol_node_is_parent_of (IdeLangservSymbolNode *self,
                                       IdeLangservSymbolNode *other)
{
  IdeLangservSymbolNodePrivate *priv = ide_langserv_symbol_node_get_instance_private (self);
  IdeLangservSymbolNodePrivate *opriv = ide_langserv_symbol_node_get_instance_private (other);

  g_return_val_if_fail (IDE_IS_LANGSERV_SYMBOL_NODE (self), FALSE);
  g_return_val_if_fail (IDE_IS_LANGSERV_SYMBOL_NODE (other), FALSE);

  return (location_compare (&priv->begin, &opriv->begin) <= 0) &&
         (location_compare (&priv->end, &opriv->end) >= 0);
}
