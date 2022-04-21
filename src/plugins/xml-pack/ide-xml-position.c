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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ide-xml-position.h"

G_DEFINE_BOXED_TYPE (IdeXmlPosition, ide_xml_position, ide_xml_position_ref, ide_xml_position_unref)

IdeXmlPosition *
ide_xml_position_new (IdeXmlSymbolNode     *node,
                      const gchar          *prefix,
                      IdeXmlPositionKind    kind,
                      IdeXmlPositionDetail  detail,
                      const gchar          *detail_name,
                      const gchar          *detail_value,
                      gchar                 quote)
{
  IdeXmlPosition *self;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (node), NULL);

  self = g_slice_new0 (IdeXmlPosition);
  self->ref_count = 1;

  self->node = (IDE_IS_XML_SYMBOL_NODE (node)) ? g_object_ref (node) : NULL;

  if (!ide_str_empty0 (prefix))
    self->prefix = g_strdup (prefix);

  if (!ide_str_empty0 (detail_name))
    self->detail_name = g_strdup (detail_name);

  if (!ide_str_empty0 (detail_value))
    self->detail_value = g_strdup (detail_value);

  self->kind = kind;
  self->detail = detail;
  self->child_pos = -1;
  self->quote = quote;

  return self;
}

IdeXmlPosition *
ide_xml_position_copy (IdeXmlPosition *self)
{
  IdeXmlPosition *copy;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  copy = ide_xml_position_new (self->node,
                               self->prefix,
                               self->kind,
                               self->detail,
                               self->detail_name,
                               self->detail_value,
                               self->quote);

  if (self->analysis != NULL)
    copy->analysis = ide_xml_analysis_ref (self->analysis);

  if (self->next_sibling_node != NULL)
    copy->next_sibling_node = g_object_ref (self->next_sibling_node);

  if (self->previous_sibling_node != NULL)
    copy->previous_sibling_node = g_object_ref (self->previous_sibling_node);

  copy->child_pos = self->child_pos;

  return copy;
}

static void
ide_xml_position_free (IdeXmlPosition *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  if (self->analysis != NULL)
    ide_xml_analysis_unref (self->analysis);

  g_clear_pointer (&self->prefix, g_free);
  g_clear_pointer (&self->detail_name, g_free);
  g_clear_pointer (&self->detail_value, g_free);

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
  if (previous_sibling_node != NULL)
    g_object_ref (previous_sibling_node);

  if (next_sibling_node != NULL)
    g_object_ref (next_sibling_node);

  self->previous_sibling_node = previous_sibling_node;
  self->next_sibling_node = next_sibling_node;
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

static const gchar *
ide_xml_position_detail_get_str (IdeXmlPositionDetail detail)
{
  const gchar *detail_str = NULL;

  switch (detail)
    {
    case IDE_XML_POSITION_DETAIL_NONE:
      detail_str = "none";
      break;

    case IDE_XML_POSITION_DETAIL_IN_NAME:
      detail_str = "in name";
      break;

    case IDE_XML_POSITION_DETAIL_IN_ATTRIBUTE_NAME:
      detail_str = "in attribute name";
      break;

    case IDE_XML_POSITION_DETAIL_IN_ATTRIBUTE_VALUE:
      detail_str = "in attribute value";
      break;

    default:
      g_assert_not_reached ();
    }

  return detail_str;
}

void
ide_xml_position_print (IdeXmlPosition *self)
{
  const gchar *p_sibling_str;
  const gchar *n_sibling_str;
  const gchar *kind_str;
  const gchar *detail_str;
  IdeXmlSymbolNode *parent_node = NULL;
  gint n_children;

  p_sibling_str = (self->previous_sibling_node == NULL) ?
    "none" :
    ide_xml_symbol_node_get_element_name (self->previous_sibling_node);

  n_sibling_str = (self->next_sibling_node == NULL) ?
    "none" :
    ide_xml_symbol_node_get_element_name (self->next_sibling_node);

  kind_str = ide_xml_position_kind_get_str (self->kind);
  detail_str = ide_xml_position_detail_get_str (self->detail);

  if (self->node != NULL)
    parent_node = ide_xml_symbol_node_get_parent (self->node);

  printf ("POSITION: parent: %s node: %s kind:%s detail:'%s'\n \
           prefix:'%s' detail name:'%s' detail value:'%s' quote:'%c'\n",
          (parent_node != NULL) ? ide_xml_symbol_node_get_element_name (parent_node) : "none",
          (self->node != NULL) ? ide_xml_symbol_node_get_element_name (self->node) : "none",
          kind_str,
          detail_str,
          self->prefix,
          self->detail_name,
          self->detail_value,
          self->quote);

  if (self->child_pos != -1)
    {
      printf (" (between %s and %s)", p_sibling_str, n_sibling_str);
      if (self->node != NULL)
        {
          n_children = ide_xml_symbol_node_get_n_direct_children (self->node);
          if (self->child_pos == 0)
            {
              if (n_children == 1)
                printf (" pos: |0\n");
              else
                printf (" pos: |0..%d\n", n_children - 1);
            }
          else if (self->child_pos == n_children)
            {
              if (n_children == 1)
                printf (" pos: 0|\n");
              else
                printf (" pos: 0..%d|\n", n_children - 1);
            }
          else
            printf (" pos: %d|%d\n", self->child_pos - 1, self->child_pos);
        }
    }
  else if (self->child_node != NULL)
    printf (" child node:%s\n", ide_xml_symbol_node_get_element_name (self->child_node));
  else
    printf ("\n");

  if (self->node != NULL)
    {
      gchar **attributes_names;
      gchar **attributes_names_cursor;
      IdeXmlSymbolNode *node;

      if (NULL != (attributes_names = ide_xml_symbol_node_get_attributes_names (self->node)))
        {
          attributes_names_cursor = attributes_names;
          while (attributes_names_cursor [0] != NULL)
            {
              g_autofree gchar *name = NULL;
              const gchar *value;

              name = g_strdup (attributes_names [0]);
              value = ide_xml_symbol_node_get_attribute_value (self->node, name);
              printf ("attr:%s=%s\n", name, value);
              ++attributes_names_cursor;
            }

          g_strfreev (attributes_names);
        }

      if ((n_children = ide_xml_symbol_node_get_n_direct_children (self->node)) > 0)
        printf ("children: %d\n", n_children);

      for (gint i = 0; i < n_children; ++i)
        {
          node = (IdeXmlSymbolNode *)ide_xml_symbol_node_get_nth_direct_child (self->node, i);
          printf ("name:'%s'\n", ide_xml_symbol_node_get_element_name (node));
        }
    }
}

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

IdeXmlPositionDetail
ide_xml_position_get_detail (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, IDE_XML_POSITION_DETAIL_NONE);

  return self->detail;
}

const gchar *
ide_xml_position_get_prefix (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);

  return self->prefix;
}

const gchar *
ide_xml_position_get_detail_name (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);

  return self->detail_name;
}

const gchar *
ide_xml_position_get_detail_value (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);

  return self->detail_value;
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

IdeXmlSymbolNode *
ide_xml_position_get_child_node (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);

  return self->child_node;
}

/* TODO: ide_xml_position_take_child_node ? */
void ide_xml_position_set_child_node (IdeXmlPosition   *self,
                                      IdeXmlSymbolNode *child_node)
{
  g_return_if_fail (self);

  g_clear_object (&self->child_node);
  self->child_node = child_node;
}
