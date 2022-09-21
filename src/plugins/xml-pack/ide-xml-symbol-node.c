/* ide-xml-symbol-node.c
 *
 * Copyright 2017 Sébastien Lafargue <slafargue@gnome.org>
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


#define G_LOG_DOMAIN "ide-xml-symbol-node"

#include "ide-xml-symbol-node.h"

typedef struct _Attribute
{
  gchar *name;
  gchar *value;
} Attribute;

typedef struct _NodeEntry
{
  IdeXmlSymbolNode *node;
  guint             is_internal : 1;
} NodeEntry;

typedef struct _NodeRange
{
  gint  start_line;
  gint  start_line_offset;
  gint  end_line;
  gint  end_line_offset;
  gsize size;
} NodeRange;

struct _IdeXmlSymbolNode
{
  IdeSymbolNode           parent_instance;

  IdeXmlSymbolNode       *parent;
  GArray                 *children;
  gchar                  *value;
  gchar                  *element_name;
  gint                    nb_children;
  gint                    nb_internal_children;
  GFile                  *file;
  GArray                 *attributes;
  gchar                  *ns;
  IdeXmlSymbolNodeState   state;
  NodeRange               start_tag;
  NodeRange               end_tag;

  guint                   has_end_tag : 1;
};

typedef enum
{
  NODE_WALKER_DIRECT_ALL,
  NODE_WALKER_INTERNAL,
  NODE_WALKER_VISIBLE_DEEP
} NodeWalker;

G_DEFINE_FINAL_TYPE (IdeXmlSymbolNode, ide_xml_symbol_node, IDE_TYPE_SYMBOL_NODE)

static void
ide_xml_symbol_node_get_location_async (IdeSymbolNode       *node,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeXmlSymbolNode *self = (IdeXmlSymbolNode *)node;
  g_autoptr(IdeTask) task = NULL;
  IdeLocation *ret;

  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));
  g_return_if_fail (G_IS_FILE (self->file));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_xml_symbol_node_get_location_async);

  ret = ide_location_new (self->file,
                          self->start_tag.start_line - 1,
                          self->start_tag.start_line_offset - 1);

  ide_task_return_pointer (task, ret, g_object_unref);
}

static IdeLocation *
ide_xml_symbol_node_get_location_finish (IdeSymbolNode  *node,
                                         GAsyncResult   *result,
                                         GError        **error)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (node), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_xml_symbol_node_finalize (GObject *object)
{
  IdeXmlSymbolNode *self = (IdeXmlSymbolNode *)object;

  g_clear_pointer (&self->children, g_array_unref);
  g_clear_pointer (&self->attributes, g_array_unref);

  g_clear_pointer (&self->element_name, g_free);
  g_clear_pointer (&self->value, g_free);

  g_clear_object (&self->file);
  g_clear_object (&self->parent);

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
  self->state = IDE_XML_SYMBOL_NODE_STATE_OK;
}

IdeXmlSymbolNode *
ide_xml_symbol_node_new (const gchar   *name,
                         const gchar   *value,
                         const gchar   *element_name,
                         IdeSymbolKind  kind)
{
  IdeXmlSymbolNode *self;
  IdeSymbolFlags flags = IDE_SYMBOL_FLAGS_NONE;

  self = g_object_new (IDE_TYPE_XML_SYMBOL_NODE,
                       "name", ide_str_empty0 (element_name) ? "unknown" : element_name,
                       "display-name", name,
                       "kind", kind,
                       "flags", flags,
                       NULL);

  if (ide_str_empty0 (element_name))
    self->element_name = g_strdup ("unknown");
  else
    self->element_name = g_strdup (element_name);

  if (!ide_str_empty0 (value))
    self->value = g_strdup (value);

  return self;
}

/* Return the number of visible chldren, walking down in the hierarchy
 * by skiping internal ones to find them.
 */
guint
ide_xml_symbol_node_get_n_children (IdeXmlSymbolNode *self)
{
  guint nb_children = 0;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), 0);

  if (self->children != NULL)
    {
      for (guint n = 0; n < self->children->len; ++n)
        {
          const NodeEntry *entry = &g_array_index (self->children, NodeEntry, n);

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

/* Return the nth_child visible node, walking down in the hierarchy
 * by skiping internal ones to find them.
 * *current_pos is use as a start position and to track the current position
 * between the recursive calls */
IdeSymbolNode *
ide_xml_symbol_node_get_nth_child_deep (IdeXmlSymbolNode *self,
                                        guint             nth_child,
                                        guint            *current_pos)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (self->children == NULL)
    return NULL;

  for (guint n = 0; n < self->children->len; ++n)
    {
      const NodeEntry *entry = &g_array_index (self->children, NodeEntry, n);
      IdeSymbolNode *node;

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
               NodeWalker        node_walker)
{
  NodeEntry *entry;
  guint pos = 0;

  if (self->children != NULL)
    {
      switch (node_walker)
        {
        case NODE_WALKER_INTERNAL:
          for (guint n = 0; n < self->children->len; ++n)
            {
              entry = &g_array_index (self->children, NodeEntry, n);
              if (entry->is_internal)
                {
                  if (pos == nth_child)
                    return (IdeSymbolNode *)g_object_ref (entry->node);

                  ++pos;
                }
            }

          break;

        case NODE_WALKER_DIRECT_ALL:
          if (nth_child < self->children->len)
            {
              entry = &g_array_index (self->children, NodeEntry, nth_child);
              return (IdeSymbolNode *)g_object_ref (entry->node);
            }

          break;

        case NODE_WALKER_VISIBLE_DEEP:
          return ide_xml_symbol_node_get_nth_child_deep (self, nth_child, &pos);
          break;

        default:
          g_assert_not_reached ();
        }
    }

  return NULL;
}

/* Return the nth_child visible node, walking down in the hierarchy
 * by skiping internal ones to find them.
 */
IdeSymbolNode *
ide_xml_symbol_node_get_nth_child (IdeXmlSymbolNode *self,
                                   guint             nth_child)
{
  IdeSymbolNode *child;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (NULL == (child = get_nth_child (self, nth_child, NODE_WALKER_VISIBLE_DEEP)))
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

  if (NULL == (child = get_nth_child (self, nth_child, NODE_WALKER_INTERNAL)))
    {
      g_warning ("nth child %u is out of bounds", nth_child);
      return NULL;
    }

  return child;
}

/* Return the number of direct chldren (visibles and internals) for this particular node */
guint
ide_xml_symbol_node_get_n_direct_children (IdeXmlSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), 0);

  return self->nb_children + self->nb_internal_children;
}

/* Return the nth_child direct node (visible or internal) */
IdeSymbolNode *
ide_xml_symbol_node_get_nth_direct_child (IdeXmlSymbolNode *self,
                                          guint             nth_child)
{
  IdeSymbolNode *child;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (NULL == (child = get_nth_child (self, nth_child, NODE_WALKER_DIRECT_ALL)))
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

  if (child != self)
    {
      if (child->parent != NULL)
        g_object_unref (child->parent);

      child->parent = g_object_ref (self);
    }
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
                                  gint              start_line,
                                  gint              start_line_offset,
                                  gint              end_line,
                                  gint              end_line_offset,
                                  gsize             size)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));
  g_return_if_fail (G_IS_FILE (file) || file == NULL);

  g_clear_object (&self->file);
  if (file != NULL)
    self->file = g_object_ref (file);

  self->start_tag.start_line = start_line;
  self->start_tag.start_line_offset = start_line_offset;
  self->start_tag.end_line = end_line;
  self->start_tag.end_line_offset = end_line_offset;
  self->start_tag.size = size;
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
                                  gint             *start_line,
                                  gint             *start_line_offset,
                                  gint             *end_line,
                                  gint             *end_line_offset,
                                  gsize            *size)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (start_line != NULL)
    *start_line = self->start_tag.start_line;

  if (start_line_offset != NULL)
    *start_line_offset = self->start_tag.start_line_offset;

  if (end_line != NULL)
    *end_line = self->start_tag.end_line;

  if (end_line_offset != NULL)
    *end_line_offset = self->start_tag.end_line_offset;

  if (size != NULL)
    *size = self->start_tag.size;

  return self->file;
}

void
ide_xml_symbol_node_get_end_tag_location (IdeXmlSymbolNode *self,
                                          gint             *start_line,
                                          gint             *start_line_offset,
                                          gint             *end_line,
                                          gint             *end_line_offset,
                                          gsize            *size)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));

  if (start_line != NULL)
    *start_line = self->end_tag.start_line;

  if (start_line_offset != NULL)
    *start_line_offset = self->end_tag.start_line_offset;

  if (end_line != NULL)
    *end_line = self->end_tag.end_line;

  if (end_line_offset != NULL)
    *end_line_offset = self->end_tag.end_line_offset;

  if (size != NULL)
    *size = self->end_tag.size;
}

void
ide_xml_symbol_node_set_end_tag_location (IdeXmlSymbolNode *self,
                                          gint              start_line,
                                          gint              start_line_offset,
                                          gint              end_line,
                                          gint              end_line_offset,
                                          gsize             size)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));
  g_return_if_fail (size >= 2);

  self->end_tag.start_line = start_line;
  self->end_tag.start_line_offset = start_line_offset;
  self->end_tag.end_line = end_line;
  self->end_tag.end_line_offset = end_line_offset;
  self->end_tag.size = size;

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

IdeXmlSymbolNodeState
ide_xml_symbol_node_get_state (IdeXmlSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self),  IDE_XML_SYMBOL_NODE_STATE_UNKNOW);

  return self->state;
}

void
ide_xml_symbol_node_set_state (IdeXmlSymbolNode      *self,
                               IdeXmlSymbolNodeState  state)
{
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));

    self->state = state;
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

IdeXmlSymbolNode *
ide_xml_symbol_node_get_parent (IdeXmlSymbolNode *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  return self->parent;
}

/* Return -1 if before, 0 if in, 1 if after the range */
static inline gint
is_in_range (NodeRange range,
             gint      line,
             gint      line_offset)
{
  if (line < range.start_line ||
      (line == range.start_line && line_offset <= range.start_line_offset))
    return -1;

  if (line > range.end_line ||
      (line == range.end_line && line_offset > range.end_line_offset))
    return 1;

  return 0;
}

static void
print_node_ranges (IdeXmlSymbolNode *node)
{
  g_print ("(%i,%i)->(%i,%i) s:%"G_GSIZE_FORMAT" end: (%i,%i)->(%i,%i) s:%"G_GSIZE_FORMAT"\n",
           node->start_tag.start_line,
           node->start_tag.start_line_offset,
           node->start_tag.end_line,
           node->start_tag.end_line_offset,
           node->start_tag.size,
           node->end_tag.start_line,
           node->end_tag.start_line_offset,
           node->end_tag.end_line,
           node->end_tag.end_line_offset,
           node->end_tag.size);
}

/* Find the relative position of the (line, line_offset) cursor
 * according to the reference node ref_node
 */
IdeXmlSymbolNodeRelativePosition
ide_xml_symbol_node_compare_location (IdeXmlSymbolNode *ref_node,
                                      gint              line,
                                      gint              line_offset)
{
  gint pos;

  pos = is_in_range(ref_node->start_tag, line, line_offset);
  if (pos == -1)
    return IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_BEFORE;
  else if (pos == 0)
    return IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_START_TAG;

  if (ref_node->has_end_tag)
    {
      pos = is_in_range(ref_node->end_tag, line, line_offset);
      if (pos == -1)
        return IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_CONTENT;
      else if (pos == 0)
        return IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_END_TAG;
    }

  return IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_AFTER;
}

void
ide_xml_symbol_node_set_attributes (IdeXmlSymbolNode  *self,
                                    const gchar      **attributes)
{
  Attribute attr;

  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));

  g_clear_pointer (&self->attributes, g_array_unref);
  if (attributes == NULL)
    return;

  self->attributes = g_array_new (FALSE, FALSE, sizeof (Attribute));
  while (attributes [0] != NULL)
    {
      attr.name = g_strdup (attributes [0]);
      attr.value = (attributes [1] != NULL) ? g_strdup (attributes [1]) : NULL;
      g_array_append_val (self->attributes, attr);
      attributes += 2;
    }
}

gchar **
ide_xml_symbol_node_get_attributes_names (IdeXmlSymbolNode  *self)
{
  Attribute *attr;
  GPtrArray *ar_names;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (self->attributes == NULL)
    return NULL;

  ar_names = g_ptr_array_new ();
  for (guint i = 0; i < self->attributes->len; ++i)
    {
      attr = &g_array_index (self->attributes, Attribute, i);
      g_ptr_array_add (ar_names, g_strdup (attr->name));
    }

  g_ptr_array_add (ar_names, NULL);

  return (gchar **)g_ptr_array_free (ar_names, FALSE);
}

const gchar *
ide_xml_symbol_node_get_attribute_value (IdeXmlSymbolNode *self,
                                         const gchar      *name)
{
  Attribute *attr;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  if (self->attributes == NULL || name == NULL)
    return NULL;

  for (guint i = 0; i < self->attributes->len; ++i)
    {
      attr = &g_array_index (self->attributes, Attribute, i);
      if (ide_str_equal0 (name, attr->name))
        return attr->value;
    }

  return NULL;
}

void
ide_xml_symbol_node_print (IdeXmlSymbolNode  *self,
                           guint              depth,
                           gboolean           recurse,
                           gboolean           show_value,
                           gboolean           show_attributes)
{
  g_autofree gchar *spacer = NULL;
  guint n_children;
  IdeXmlSymbolNode *child;
  Attribute *attr;

  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (self));

  if (self == NULL)
    {
      g_warning ("Node NULL");
      return;
    }

  spacer = g_strnfill (depth, '\t');
  g_print ("%s%s state:%d ", spacer, self->element_name, self->state);
  print_node_ranges (self);

  if (show_attributes && self->attributes != NULL)
    {
      for (guint i = 0; i < self->attributes->len; ++i)
        {
          attr = &g_array_index (self->attributes, Attribute, i);
          g_print ("attr '%s':'%s'\n", attr->name, attr->value);
        }
    }

  if (show_value && self->value != NULL)
    g_print ("%svalue:%s\n", spacer, self->value);

  if (recurse)
    {
      n_children = ide_xml_symbol_node_get_n_direct_children (self);
      for (guint i = 0; i < n_children; ++i)
        {
          child = (IdeXmlSymbolNode *)ide_xml_symbol_node_get_nth_direct_child (self, i);
          ide_xml_symbol_node_print (child, depth + 1, recurse, show_value, show_attributes);
        }
    }
}

const gchar *
ide_xml_symbol_node_get_namespace (IdeXmlSymbolNode  *self)
{
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (self), NULL);

  return self->ns;
}
