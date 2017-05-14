/* ide-xml-path.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include "ide-xml-path.h"

G_DEFINE_BOXED_TYPE (IdeXmlPath, ide_xml_path, ide_xml_path_ref, ide_xml_path_unref)

void
ide_xml_path_append_node (IdeXmlPath *self,
                          xmlNode    *node)
{
  g_return_if_fail (self);
  g_return_if_fail (node);

  g_ptr_array_add (self->nodes, node);
}

void
ide_xml_path_prepend_node (IdeXmlPath *self,
                           xmlNode    *node)
{
  g_return_if_fail (self);
  g_return_if_fail (node);

  g_ptr_array_insert (self->nodes, 0, node);
}

void
ide_xml_path_dump (IdeXmlPath *self)
{
  xmlNode *node;
  const gchar *type_name;

  g_return_if_fail (self);

  for (gint i = 0; i < self->nodes->len; ++i)
    {
      node = g_ptr_array_index (self->nodes, i);
      if (node->type == XML_ELEMENT_NODE)
        type_name = "element";
      else if (node->type == XML_ATTRIBUTE_NODE)
        type_name = "attribute";
      else if (node->type == XML_TEXT_NODE)
        type_name = "text";
      else if (node->type == XML_CDATA_SECTION_NODE)
        type_name = "cdata";
      else if (node->type == XML_PI_NODE)
        type_name = "PI";
      else
        type_name = "----";

      if (node->name != NULL)
        printf ("%s: %s\n", type_name, node->name);
      else
        printf ("%s\n", type_name);
    }
}

IdeXmlPath *
ide_xml_path_new (void)
{
  IdeXmlPath *self;

  self = g_slice_new0 (IdeXmlPath);
  self->ref_count = 1;

  self->nodes = g_ptr_array_sized_new (8);

  return self;
}

IdeXmlPath *
ide_xml_path_new_from_node (xmlNode *node)
{
  IdeXmlPath *self;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (node, NULL);

  self = ide_xml_path_new ();

  do
    {
      ide_xml_path_append_node (self, node);
      node = node->parent;
    } while (node != NULL);

  return self;
}

IdeXmlPath *
ide_xml_path_copy (IdeXmlPath *self)
{
  IdeXmlPath *copy;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  copy = ide_xml_path_new ();

  return copy;
}

static void
ide_xml_path_free (IdeXmlPath *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  g_ptr_array_unref (self->nodes);

  g_slice_free (IdeXmlPath, self);
}

IdeXmlPath *
ide_xml_path_ref (IdeXmlPath *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_path_unref (IdeXmlPath *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_path_free (self);
}
