/* ide-xml-path.c
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

#include "ide-xml-path.h"

G_DEFINE_BOXED_TYPE (IdeXmlPath, ide_xml_path, ide_xml_path_ref, ide_xml_path_unref)

void
ide_xml_path_append_node (IdeXmlPath       *self,
                          IdeXmlSymbolNode *node)
{
  g_return_if_fail (self);
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (node));

  g_ptr_array_add (self->nodes, g_object_ref (node));
}

void
ide_xml_path_prepend_node (IdeXmlPath       *self,
                           IdeXmlSymbolNode *node)
{
  g_return_if_fail (self);
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (node));

  g_ptr_array_insert (self->nodes, 0, g_object_ref (node));
}

void
ide_xml_path_dump (IdeXmlPath *self)
{
  g_return_if_fail (self);

  for (gint i = 0; i < self->nodes->len; ++i)
    {
      IdeXmlSymbolNode *node = g_ptr_array_index (self->nodes, i);

      ide_xml_symbol_node_print (node, 0, FALSE, TRUE, TRUE);
    }
}

IdeXmlPath *
ide_xml_path_new (void)
{
  IdeXmlPath *self;

  self = g_slice_new0 (IdeXmlPath);
  self->ref_count = 1;

  self->nodes = g_ptr_array_new_full (8, g_object_unref);

  return self;
}

IdeXmlPath *
ide_xml_path_new_from_node (IdeXmlSymbolNode *node)
{
  IdeXmlPath *self;

  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (node), NULL);

  self = ide_xml_path_new ();

  do
    {
      ide_xml_path_append_node (self, node);
      node = ide_xml_symbol_node_get_parent (node);
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

  g_clear_pointer (&self->nodes, g_ptr_array_unref);

  g_slice_free (IdeXmlPath, self);
}

IdeXmlPath *
ide_xml_path_ref (IdeXmlPath *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_path_unref (IdeXmlPath *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_path_free (self);
}
