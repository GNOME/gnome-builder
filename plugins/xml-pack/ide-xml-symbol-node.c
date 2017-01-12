/* ide-xml-symbol-node.c
 *
 * Copyright (C) 2017 Sébastien Lafargue <slafargue@gnome.org>
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


#define G_LOG_DOMAIN "ide-xml-symbol-node"

#include "ide-xml-symbol-node.h"

struct _IdeXmlSymbolNode
{
  IdeSymbolNode             parent_instance;
  GPtrArray                *children;
  GFile                    *file;
  guint                     line;
  guint                     line_offset;
  gint64                    serial;
};

G_DEFINE_TYPE (IdeXmlSymbolNode, ide_xml_symbol_node, IDE_TYPE_SYMBOL_NODE)

static void
ide_xml_symbol_node_get_location_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeXmlSymbolResolver *resolver = (IdeXmlSymbolResolver *)object;
  g_autoptr(IdeSourceLocation) location = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_XML_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_TASK (task));

  //location = ide_xml_symbol_resolver_get_location_finish (resolver, result, &error);

  if (location == NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task,
                           g_steal_pointer (&location),
                           (GDestroyNotify)ide_source_location_unref);
}

static void
ide_xml_symbol_node_get_location_async (IdeSymbolNode       *node,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeXmlSymbolNode *self = (IdeXmlSymbolNode *)node;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_xml_symbol_node_get_location_async);

  /* ide_xml_symbol_resolver_get_location_async (self->resolver, */
  /*                                             self->index, */
  /*                                             self->entry, */
  /*                                             NULL, */
  /*                                             ide_xml_symbol_node_get_location_cb, */
  /*                                             g_steal_pointer (&task)); */
}

static IdeSourceLocation *
ide_xml_symbol_node_get_location_finish (IdeSymbolNode  *node,
                                         GAsyncResult   *result,
                                         GError        **error)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (node), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_xml_symbol_node_finalize (GObject *object)
{
  IdeXmlSymbolNode *self = (IdeXmlSymbolNode *)object;

  g_clear_pointer (&self->children, g_ptr_array_unref);
  /* self->entry = NULL; */
  /* g_clear_object (&self->index); */

  G_OBJECT_CLASS (ide_xml_symbol_node_parent_class)->finalize (object);
}

static void
ide_xml_symbol_node_class_init (IdeXmlSymbolNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSymbolNodeClass *symbol_node_class = IDE_SYMBOL_NODE_CLASS (klass);

  object_class->finalize = ide_xml_symbol_node_finalize;

  symbol_node_class->get_location_async = ide_xml_symbol_node_get_location_async;
  symbol_node_class->get_location_finish = ide_xml_symbol_node_get_location_finish;
}

static void
ide_xml_symbol_node_init (IdeXmlSymbolNode *self)
{
}

IdeXmlSymbolNode *
ide_xml_symbol_node_new (const gchar            *name,
                         GFile                  *file,
                         guint                   line,
                         guint                   line_offset)
{
  IdeXmlSymbolNode *self;
  IdeSymbolFlags flags = IDE_SYMBOL_FLAGS_NONE;

  g_assert (!ide_str_empty0 (name));
  //g_assert (G_IS_FILE (file));

  self = g_object_new (IDE_TYPE_XML_SYMBOL_NODE,
                       "name", name,
                       "kind", IDE_SYMBOL_NONE,
                       "flags", flags,
                       NULL);

  self->file = file;
  self->line = line;
  self->line_offset = line_offset;

  return self;
}

guint
ide_xml_symbol_node_get_n_children (IdeXmlSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), 0);

  return self->children != NULL ? self->children->len : 0;
}

IdeSymbolNode *
ide_xml_symbol_node_get_nth_child (IdeXmlSymbolNode *self,
                                   guint             nth_child)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (self->children != NULL && nth_child < self->children->len)
    return g_object_ref (g_ptr_array_index (self->children, nth_child));

  return NULL;
}

void
ide_xml_symbol_node_take_child (IdeXmlSymbolNode *self,
                                IdeXmlSymbolNode *child)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (child));

  if (self->children == NULL)
    self->children = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (self->children, child);
}

gint64
ide_xml_symbol_node_get_serial (IdeXmlSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), -1);

  return self->serial;
}
