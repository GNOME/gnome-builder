/* ide-xml-stack.c
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

#include <libide-code.h>

#include "ide-xml-stack.h"

typedef struct _StackItem
{
  gchar            *name;
  IdeXmlSymbolNode *node;
  IdeXmlSymbolNode *parent;
  gint              depth;
} StackItem;

struct _IdeXmlStack
{
  GObject  parent_instance;

  GArray  *array;
};

G_DEFINE_FINAL_TYPE (IdeXmlStack, ide_xml_stack, G_TYPE_OBJECT)

IdeXmlStack *
ide_xml_stack_new (void)
{
  return g_object_new (IDE_TYPE_XML_STACK, NULL);
}

void
ide_xml_stack_push (IdeXmlStack       *self,
                    const gchar       *name,
                    IdeXmlSymbolNode  *node,
                    IdeXmlSymbolNode  *parent,
                    gint               depth)
{
  StackItem item;

  g_return_if_fail (IDE_IS_XML_STACK (self));
  g_return_if_fail (!ide_str_empty0 (name));
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (node) || node == NULL);
  g_return_if_fail (IDE_IS_XML_SYMBOL_NODE (parent) || parent == NULL);

  item.name = g_strdup (name);
  item.node = node;
  item.parent = parent;
  item.depth = depth;

  g_array_append_val (self->array, item);
}

IdeXmlSymbolNode *
ide_xml_stack_pop (IdeXmlStack       *self,
                   gchar            **name,
                   IdeXmlSymbolNode **parent,
                   gint              *depth)
{
  StackItem *item;
  IdeXmlSymbolNode *node;
  gsize last;

  g_return_val_if_fail (IDE_IS_XML_STACK (self), NULL);

  if (self->array->len == 0)
    return NULL;

  last = self->array->len - 1;
  item = &g_array_index (self->array, StackItem, last);
  node = item->node;

  if (depth != NULL)
    *depth = item->depth;

  if (name != NULL)
    *name = (item->name != NULL) ? g_steal_pointer (&item->name) : NULL;

  if (parent != NULL)
    *parent = item->parent;

  self->array = g_array_remove_index (self->array, last);
  return node;
}

IdeXmlSymbolNode *
ide_xml_stack_peek (IdeXmlStack       *self,
                    const gchar      **name,
                    IdeXmlSymbolNode **parent,
                    gint              *depth)
{
  StackItem *item;
  IdeXmlSymbolNode *node;
  gsize last;

  g_return_val_if_fail (IDE_IS_XML_STACK (self), NULL);

  if (self->array->len == 0)
    return NULL;

  last = self->array->len - 1;
  item = &g_array_index (self->array, StackItem, last);
  node = item->node;

  if (depth != NULL)
    *depth = item->depth;

  if (name != NULL)
    *name = item->name;

  if (parent != NULL)
    *parent = item->parent;

  return node;
}

gsize
ide_xml_stack_get_size (IdeXmlStack *self)
{
  g_return_val_if_fail (IDE_IS_XML_STACK (self), 0);

  return self->array->len;
}

gboolean
ide_xml_stack_is_empty (IdeXmlStack *self)
{
  g_return_val_if_fail (IDE_IS_XML_STACK (self), TRUE);

  return (self->array->len == 0);
}

static void
clear_array (gpointer data)
{
  StackItem *item = (StackItem *)data;

  g_free (item->name);
}

static void
ide_xml_stack_finalize (GObject *object)
{
  IdeXmlStack *self = (IdeXmlStack *)object;

  g_clear_pointer (&self->array, g_array_unref);

  G_OBJECT_CLASS (ide_xml_stack_parent_class)->finalize (object);
}

static void
ide_xml_stack_class_init (IdeXmlStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_stack_finalize;
}

static void
ide_xml_stack_init (IdeXmlStack *self)
{
  self->array = g_array_new (FALSE, TRUE, sizeof (StackItem));
  g_array_set_clear_func (self->array, (GDestroyNotify)clear_array);
}
