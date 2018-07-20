/* ide-xml-position.c
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include "ide-xml-position.h"

G_DEFINE_BOXED_TYPE (IdeXmlPosition, ide_xml_position, ide_xml_position_ref, ide_xml_position_unref)

IdeXmlPosition *
ide_xml_position_new (IdeXmlSymbolNode   *node,
                      IdeXmlPositionKind  kind,
                      IdeXmlDetail       *detail)
{
  IdeXmlPosition *self;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (node), NULL);

  self = g_slice_new0 (IdeXmlPosition);
  self->ref_count = 1;

  self->node = g_object_ref (node);

  if (detail != NULL)
    self->detail = ide_xml_detail_ref (detail);

  self->kind = kind;
  self->child_pos = -1;

  return self;
}

IdeXmlPosition *
ide_xml_position_copy (IdeXmlPosition *self)
{
  IdeXmlPosition *copy;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  copy = ide_xml_position_new (self->node,
                               self->kind,
                               self->detail);

  ide_xml_position_set_analysis (copy, self->analysis);
  ide_xml_position_set_siblings (copy, self->previous_sibling_node, self->next_sibling_node);
  copy->child_pos = self->child_pos;

  return copy;
}

static void
ide_xml_position_free (IdeXmlPosition *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  g_clear_pointer (&self->analysis, ide_xml_analysis_unref);
  g_clear_pointer (&self->detail, ide_xml_detail_unref);

  g_clear_object (&self->node);
  g_clear_object (&self->child_node);
  g_clear_object (&self->previous_sibling_node);
  g_clear_object (&self->next_sibling_node);

  g_slice_free (IdeXmlPosition, self);
}

IdeXmlPosition *
ide_xml_position_ref (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_position_unref (IdeXmlPosition *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_position_free (self);
}

/**
 * ide_xml_position_get_analysis:
 *
 * Get the #IdeXmlAnalysis object.
 *
 * Returns: (transfer none): an #IdeXmlAnalysis
 */
IdeXmlAnalysis *
ide_xml_position_get_analysis (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);

  return self->analysis;
}

void
ide_xml_position_set_analysis (IdeXmlPosition *self,
                               IdeXmlAnalysis *analysis)
{
  g_return_if_fail (self);
  g_return_if_fail (analysis);

  g_clear_pointer (&self->analysis, ide_xml_analysis_unref);
  self->analysis = ide_xml_analysis_ref (analysis);
}

IdeXmlSymbolNode *
ide_xml_position_get_next_sibling (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);

  return self->next_sibling_node;
}

IdeXmlSymbolNode *
ide_xml_position_get_previous_sibling (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);

  return self->previous_sibling_node;
}

void
ide_xml_position_set_siblings    (IdeXmlPosition   *self,
                                  IdeXmlSymbolNode *previous_sibling_node,
                                  IdeXmlSymbolNode *next_sibling_node)
{
  g_return_if_fail (self);

  g_set_object (&self->previous_sibling_node, previous_sibling_node);
  g_set_object (&self->next_sibling_node, next_sibling_node);
}

const gchar *
ide_xml_position_kind_get_str (IdeXmlPositionKind kind)
{
  const gchar *kind_str = NULL;

  switch (kind)
    {
    case IDE_XML_POSITION_KIND_UNKNOW:
      kind_str = "unknow";
      break;

    case IDE_XML_POSITION_KIND_IN_START_TAG:
      kind_str = "in start";
      break;

    case IDE_XML_POSITION_KIND_IN_END_TAG:
      kind_str = "in end";
      break;

    case IDE_XML_POSITION_KIND_IN_CONTENT:
      kind_str = "in content";
      break;

    default:
      g_assert_not_reached ();
    }

  return kind_str;
}

void
ide_xml_position_print (IdeXmlPosition *self)
{
  const gchar *p_sibling_str;
  const gchar *n_sibling_str;
  const gchar *kind_str;
  IdeXmlSymbolNode *parent_node = NULL;
  gint n_children;

  g_return_if_fail (self);

  p_sibling_str = (self->previous_sibling_node == NULL) ?
    "none" :
    ide_xml_symbol_node_get_element_name (self->previous_sibling_node);

  n_sibling_str = (self->next_sibling_node == NULL) ?
    "none" :
    ide_xml_symbol_node_get_element_name (self->next_sibling_node);

  kind_str = ide_xml_position_kind_get_str (self->kind);

  if (self->node != NULL)
    parent_node = ide_xml_symbol_node_get_parent (self->node);

  g_print ("POSITION: parent: %s node: %s kind:%s\n",
           (parent_node != NULL) ? ide_xml_symbol_node_get_element_name (parent_node) : "none",
           (self->node != NULL) ? ide_xml_symbol_node_get_element_name (self->node) : "none",
           kind_str);

  if (self->detail != NULL)
    ide_xml_detail_print (self->detail);

  if (self->child_pos != -1)
    {
      g_print (" (between %s and %s)", p_sibling_str, n_sibling_str);
      if (self->node != NULL)
        {
          n_children = ide_xml_symbol_node_get_n_direct_children (self->node);
          if (self->child_pos == 0)
            {
              if (n_children == 1)
                g_print (" pos: |0\n");
              else if (n_children == 0)
                g_print (" pos: |\n");
              else
                g_print (" pos: |0..%d\n", n_children - 1);
            }
          else if (self->child_pos == n_children)
            {
              if (n_children == 1)
                g_print (" pos: 0|\n");
              else
                g_print (" pos: 0..%d|\n", n_children - 1);
            }
          else
            g_print (" pos: %d|%d\n", self->child_pos - 1, self->child_pos);
        }
    }
  else if (self->child_node != NULL)
    g_print (" child node:%s\n", ide_xml_symbol_node_get_element_name (self->child_node));
  else
    g_print ("\n");

  if (self->node != NULL)
    {
      gchar **attributes_names;
      gchar **attributes_names_cursor;
      IdeXmlSymbolNode *node;

      if ((attributes_names = ide_xml_symbol_node_get_attributes_names (self->node)))
        {
          attributes_names_cursor = attributes_names;
          while (attributes_names_cursor [0] != NULL)
            {
              g_autofree gchar *name = NULL;
              const gchar *value;

              name = g_strdup (attributes_names [0]);
              value = ide_xml_symbol_node_get_attribute_value (self->node, name);
              g_print ("attr:%s=%s\n", name, value);
              ++attributes_names_cursor;
            }

          g_strfreev (attributes_names);
        }

      if ((n_children = ide_xml_symbol_node_get_n_direct_children (self->node)) > 0)
        g_print ("children: %d\n", n_children);

      for (gint i = 0; i < n_children; ++i)
        {
          node = (IdeXmlSymbolNode *)ide_xml_symbol_node_get_nth_direct_child (self->node, i);
          g_print ("name:'%s'\n", ide_xml_symbol_node_get_element_name (node));
        }
    }
}

/**
 * ide_xml_position_get_node:
 *
 * Get the node part of the position.
 *
 * Returns: (transfer none): an #IdeXmlSymbolNode
 */
IdeXmlSymbolNode *
ide_xml_position_get_node (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);

  return self->node;
}

IdeXmlPositionKind
ide_xml_position_get_kind (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, IDE_XML_POSITION_KIND_UNKNOW);

  return self->kind;
}

/**
 * ide_xml_position_get_detail:
 *
 * Get the detail part of the  position.
 *
 * Returns: (transfer none): an #IdeXmldetail
 */
IdeXmlDetail *
ide_xml_position_get_detail (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);

  return self->detail;
}

gint
ide_xml_position_get_child_pos (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, -1);

  return self->child_pos;
}

void ide_xml_position_set_child_pos (IdeXmlPosition *self,
                                     gint            child_pos)
{
  g_return_if_fail (self);

  self->child_pos = child_pos;
}

/**
 * ide_xml_position_get_child_node:
 *
 *  Get the #IdeXmlSymbolNode child node.
 *
 * Returns: (transfer none): an #IdeXmlSymbolNode
 */
IdeXmlSymbolNode *
ide_xml_position_get_child_node (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);

  return self->child_node;
}

void ide_xml_position_set_child_node (IdeXmlPosition   *self,
                                      IdeXmlSymbolNode *child_node)
{
  g_return_if_fail (self);

  g_set_object (&self->child_node, child_node);
}
