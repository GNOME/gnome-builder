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

typedef struct _NodeEntry
{
  IdeXmlSymbolNode *node;
  guint             is_internal : 1;
} NodeEntry;

struct _IdeXmlSymbolNode
{
  IdeSymbolNode             parent_instance;
  GArray                   *children;
  gchar                    *value;
  gchar                    *element_name;
  gint                      nb_children;
  gint                      nb_internal_children;
  GFile                    *file;
  gint                      line;
  gint                      line_offset;
  gsize                     size;

  gint                      end_line;
  gint                      end_line_offset;
  gsize                     end_size;

  guint                     has_end_tag : 1;
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

  g_clear_pointer (&self->children, g_array_unref);

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
  NodeEntry *entry;
  guint nb_children = 0;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), 0);

  if (self->children != NULL)
    {
      for (gint n = 0; n < self->children->len; ++n)
        {
          entry = &g_array_index (self->children, NodeEntry, n);
          if (entry->is_internal)
            {
              nb_children += ide_xml_symbol_node_get_n_children (entry->node);
              continue;
            }

          ++nb_children;
        }
    }

  return nb_children;
}

IdeSymbolNode *
ide_xml_symbol_node_get_nth_child_deep (IdeXmlSymbolNode *self,
                                        guint             nth_child,
                                        guint            *current_pos)
{
  IdeSymbolNode *node;
  NodeEntry *entry;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (self->children == NULL)
    return NULL;

  for (gint n = 0; n < self->children->len; ++n)
    {
      entry = &g_array_index (self->children, NodeEntry, n);
      if (entry->is_internal)
        {
          node = ide_xml_symbol_node_get_nth_child_deep (entry->node, nth_child, current_pos);
          if (node == NULL)
            continue;

          return IDE_SYMBOL_NODE (g_object_ref (node));
        }
      else
        node = IDE_SYMBOL_NODE (entry->node);

      if (*current_pos == nth_child)
        return IDE_SYMBOL_NODE (g_object_ref (node));

      ++(*current_pos);
    }

  return NULL;
}

static IdeSymbolNode *
get_nth_child (IdeXmlSymbolNode *self,
               guint             nth_child,
               gboolean          internal)
{
  NodeEntry *entry;
  guint pos = 0;

  if (self->children != NULL)
    {
      if (internal)
        {
          for (gint n = 0; n < self->children->len; ++n)
            {
              entry = &g_array_index (self->children, NodeEntry, n);
              if (entry->is_internal)
                {
                  if (pos == nth_child)
                    return (IdeSymbolNode *)g_object_ref (entry->node);

                  ++pos;
                }
            }
        }
      else
        return ide_xml_symbol_node_get_nth_child_deep (self, nth_child, &pos);
    }

  return NULL;
}

IdeSymbolNode *
ide_xml_symbol_node_get_nth_child (IdeXmlSymbolNode *self,
                                   guint             nth_child)
{
  IdeSymbolNode *child;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (NULL == (child = get_nth_child (self, nth_child, FALSE)))
    {
      g_warning ("nth child %u is out of bounds", nth_child);
      return NULL;
    }

  return child;
}

guint
ide_xml_symbol_node_get_n_internal_children (IdeXmlSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), 0);

  return self->nb_internal_children;
}

IdeSymbolNode *
ide_xml_symbol_node_get_nth_internal_child (IdeXmlSymbolNode *self,
                                            guint             nth_child)
{
  IdeSymbolNode *child;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (NULL == (child = get_nth_child (self, nth_child, TRUE)))
    {
      g_warning ("nth child %u is out of bounds", nth_child);
      return NULL;
    }

  return child;
}

static void
node_entry_free (gpointer data)
{
  NodeEntry *node_entry = (NodeEntry *)data;

  g_assert (node_entry->node != NULL);

  g_object_unref (node_entry->node);
}

static void
take_child (IdeXmlSymbolNode *self,
            IdeXmlSymbolNode *child,
            gboolean          is_internal)
{
  NodeEntry node_entry;

  if (self->children == NULL)
    {
      self->children = g_array_new (FALSE, TRUE, sizeof (NodeEntry));
      g_array_set_clear_func (self->children, node_entry_free);
    }

  node_entry.node = child;
  node_entry.is_internal = is_internal;

  g_array_append_val (self->children, node_entry);
}

void
ide_xml_symbol_node_take_child (IdeXmlSymbolNode *self,
                                IdeXmlSymbolNode *child)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (child));

  take_child (self, child, FALSE);
  ++self->nb_children;
}

void
ide_xml_symbol_node_take_internal_child (IdeXmlSymbolNode *self,
                                         IdeXmlSymbolNode *child)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (child));

  take_child (self, child, TRUE);
  ++self->nb_internal_children;
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

void
ide_xml_symbol_node_get_end_tag_location (IdeXmlSymbolNode *self,
                                          gint             *end_line,
                                          gint             *end_line_offset,
                                          gsize            *end_size)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));

  if (end_line != NULL)
    *end_line = self->end_line;

  if (end_line_offset != NULL)
    *end_line_offset = self->end_line_offset;

  if (end_size != NULL)
    *end_size = self->end_size;
}

void
ide_xml_symbol_node_set_end_tag_location (IdeXmlSymbolNode *self,
                                          gint              end_line,
                                          gint              end_line_offset,
                                          gsize             end_size)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));

  self->end_line = end_line;
  self->end_line_offset = end_line_offset;
  self->end_size = end_size;

  self->has_end_tag = TRUE;
}

gboolean
ide_xml_symbol_node_has_end_tag (IdeXmlSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), FALSE);

  return self->has_end_tag;
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
