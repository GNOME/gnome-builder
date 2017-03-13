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
  GPtrArray                *internal_children;
  gchar                    *value;
  gchar                    *element_name;
  GFile                    *file;
  gint                      line;
  gint                      line_offset;
  gsize                     size;
};

G_DEFINE_TYPE (IdeXmlSymbolNode, ide_xml_symbol_node, IDE_TYPE_SYMBOL_NODE)

static void
ide_xml_symbol_node_get_location_async (IdeSymbolNode       *node,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeXmlSymbolNode *self = (IdeXmlSymbolNode *)node;
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  g_autoptr(IdeFile) ifile = NULL;
  IdeSourceLocation *ret;

  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));
  g_return_if_fail (G_IS_FILE (self->file));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_xml_symbol_node_get_location_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  ifile = g_object_new (IDE_TYPE_FILE,
                        "file", self->file,
                        "context", context,
                        NULL);

  /* TODO: libxml2 give us the end of a tag, we need to walk back
   * in the file content to get the start
   */
  ret = ide_source_location_new (ifile, self->line - 1, self->line_offset - 1, 0);

  g_task_return_pointer (task, ret, (GDestroyNotify)ide_source_location_unref);
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
  g_clear_pointer (&self->internal_children, g_ptr_array_unref);

  g_clear_pointer (&self->element_name, g_free);
  g_clear_pointer (&self->value, g_free);

  g_clear_object (&self->file);

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
                         const gchar            *value,
                         const gchar            *element_name,
                         IdeSymbolKind           kind,
                         GFile                  *file,
                         gint                    line,
                         gint                    line_offset,
                         gsize                   size)
{
  IdeXmlSymbolNode *self;
  IdeSymbolFlags flags = IDE_SYMBOL_FLAGS_NONE;

  g_return_val_if_fail (!ide_str_empty0 (name), NULL);
  g_return_val_if_fail (G_IS_FILE (file)|| file == NULL, NULL);

  self = g_object_new (IDE_TYPE_XML_SYMBOL_NODE,
                       "name", name,
                       "kind", kind,
                       "flags", flags,
                       NULL);

  if (ide_str_empty0 (element_name))
    self->element_name = g_strdup ("unknow");
  else
    self->element_name = g_strdup (element_name);

  if (!ide_str_empty0 (value))
    self->value = g_strdup (value);

  if (file != NULL)
    self->file = g_object_ref (file);

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

  g_warning ("nth child %u is out of bounds", nth_child);
  return NULL;
}

guint
ide_xml_symbol_node_get_n_internal_children (IdeXmlSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), 0);

  return self->internal_children != NULL ? self->internal_children->len : 0;
}

IdeSymbolNode *
ide_xml_symbol_node_get_nth_internal_child (IdeXmlSymbolNode *self,
                                            guint             nth_child)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (self->internal_children != NULL && nth_child < self->internal_children->len)
    return g_object_ref (g_ptr_array_index (self->internal_children, nth_child));

  g_warning ("nth child %u is out of bounds", nth_child);
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

void
ide_xml_symbol_node_take_internal_child (IdeXmlSymbolNode *self,
                                         IdeXmlSymbolNode *child)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (child));

  if (self->internal_children == NULL)
    self->internal_children = g_ptr_array_new_with_free_func (g_object_unref);

  g_ptr_array_add (self->internal_children, child);
}

void
ide_xml_symbol_node_set_location (IdeXmlSymbolNode *self,
                                  GFile            *file,
                                  gint              line,
                                  gint              line_offset,
                                  gsize             size)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));
  g_return_if_fail (G_IS_FILE (file) || file == NULL);

  g_clear_object (&self->file);
  if (file != NULL)
    self->file = g_object_ref (file);

  self->line = line;
  self->line_offset = line_offset;
  self->size = size;
}

/**
 * ide_xml_symbol_node_get_location:
 * @self: An #IdeXmlSymbolNode.
 *
 * Gets the location of an xml node.
 *
 * Returns: (transfer none): Gets the location of an xml node.
 */
GFile *
ide_xml_symbol_node_get_location (IdeXmlSymbolNode *self,
                                  gint             *line,
                                  gint             *line_offset,
                                  gsize            *size)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (line != NULL)
    *line = self->line;

  if (line_offset != NULL)
    *line_offset = self->line_offset;

  if (size != NULL)
    *size = self->size;

  return self->file;
}

const gchar *
ide_xml_symbol_node_get_element_name (IdeXmlSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  return self->element_name;
}

void
ide_xml_symbol_node_set_element_name (IdeXmlSymbolNode *self,
                                      const gchar      *element_name)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));
  g_return_if_fail (!ide_str_empty0 (element_name));

  g_clear_pointer (&self->element_name, g_free);

  if (element_name != NULL)
    self->element_name = g_strdup (element_name);
}

const gchar *
ide_xml_symbol_node_get_value (IdeXmlSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  return self->value;
}

void
ide_xml_symbol_node_set_value (IdeXmlSymbolNode *self,
                               const gchar      *value)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));

  g_clear_pointer (&self->value, g_free);

  if (value != NULL)
    self->value = g_strdup (value);
}
