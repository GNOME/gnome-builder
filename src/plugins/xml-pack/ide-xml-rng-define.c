/* ide-xml-rng-define.c
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

#include "ide-xml-rng-define.h"

G_DEFINE_BOXED_TYPE (IdeXmlRngDefine, ide_xml_rng_define, ide_xml_rng_define_ref, ide_xml_rng_define_unref)

static const gchar *type_names [] = {
  "noop",
  "define",
  "empty",
  "not allowed",
  "text",
  "element",
  "datatype",
  "value",
  "list",
  "ref",
  "parent ref",
  "external ref",
  "zero or more",
  "one or more",
  "optional",
  "choice",
  "group",
  "attribute group",
  "interleave",
  "attribute",
  "start",
  "param",
  "except"
};

const gchar *
ide_xml_rng_define_get_type_name (IdeXmlRngDefine *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return type_names [self->type];
}

static void
dump_tree (IdeXmlRngDefine *self,
           gint             indent)
{
  IdeXmlRngDefine *def = self;
  const gchar *type_name;
  g_autofree gchar *pad = NULL;
  guchar *name = NULL;

  pad = g_strnfill (indent, ' ');
  while (def != NULL)
    {
      type_name = ide_xml_rng_define_get_type_name (def);
      if (def->type == IDE_XML_RNG_DEFINE_REF ||
          def->type == IDE_XML_RNG_DEFINE_PARENTREF ||
          def->type == IDE_XML_RNG_DEFINE_EXTERNALREF)
        {
          if (def->node != NULL &&
              NULL != (name = xmlGetProp (def->node, (const guchar *)"name")))
            {
              printf ("%s%s [%s]:%p\n", pad, type_name, name, def->content);
              xmlFree (name);
            }
          else
            printf ("%s%s: %p\n", pad, type_name, def->content);
        }
      else
        {
          if (def->name == NULL)
            printf ("%s%s\n", pad, type_name);
          else
            printf ("%s%s [%s]\n", pad, type_name, def->name);

          if (def->content != NULL)
            {
              printf ("%s>content:\n", pad);
              dump_tree (def->content, indent + 1);
            }

          if (def->attributes != NULL)
            {
              printf ("%s>attributes:\n", pad);
              dump_tree (def->attributes, indent + 1);
            }

          if (def->name_class != NULL)
            {
              printf ("%s>name classes:\n", pad);
              dump_tree (def->name_class, indent + 1);
            }
        }

      def = def->next;
    }
}

void
ide_xml_rng_define_dump_tree (IdeXmlRngDefine *self,
                              gboolean         recursive)
{
  const gchar *type_name;

  g_return_if_fail (self != NULL);

  if (recursive)
    dump_tree (self, 0);
  else
    {
      type_name = type_names [self->type];
      if (self->name == NULL)
        printf ("%s\n", type_name);
      else
        printf ("%s [%s]\n", type_name, self->name);
    }
}

IdeXmlRngDefine *
ide_xml_rng_define_new (xmlNode             *node,
                        IdeXmlRngDefine     *parent,
                        const guchar        *name,
                        IdeXmlRngDefineType  type)
{
  IdeXmlRngDefine *self;

  self = g_slice_new0 (IdeXmlRngDefine);
  self->ref_count = 1;

  if (name != NULL)
    self->name = (guchar *)xmlStrdup (name);

  self->type = type;
  self->node = node;
  self->parent = parent;

  return self;
}

static void
ide_xml_rng_define_free (IdeXmlRngDefine *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  if (self->name != NULL)
    xmlFree (self->name);

  if (self->ns != NULL)
    xmlFree (self->ns);

  if (self->next != NULL)
    ide_xml_rng_define_unref (self->next);

  if (self->content != NULL)
    ide_xml_rng_define_unref (self->content);

  if (self->attributes != NULL)
    ide_xml_rng_define_unref (self->attributes);

  if (self->name_class != NULL)
    ide_xml_rng_define_unref (self->name_class);

  g_slice_free (IdeXmlRngDefine, self);
}

IdeXmlRngDefine *
ide_xml_rng_define_ref (IdeXmlRngDefine *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_rng_define_unref (IdeXmlRngDefine *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_rng_define_free (self);
}

void
ide_xml_rng_define_append (IdeXmlRngDefine *self,
                           IdeXmlRngDefine *def)
{
  IdeXmlRngDefine *last = self;

  g_return_if_fail (self);
  g_return_if_fail (def);

  while (last->next != NULL)
    last = last->next;

  last->next = def;
}

void
ide_xml_rng_define_propagate_parent (IdeXmlRngDefine *self,
                                     IdeXmlRngDefine *parent)
{
  IdeXmlRngDefine *last = self;

  g_return_if_fail (self);

  do
    {
      last->parent = parent;
      last = last->next;
    } while (last != NULL);
}

gboolean
ide_xml_rng_define_is_nameclass_match (IdeXmlRngDefine  *define,
                                       IdeXmlSymbolNode *node)
{
  const gchar *name;
  const gchar *namespace;
  IdeXmlRngDefine *nc;
  IdeXmlRngDefine *content;

  g_assert (IDE_IS_XML_SYMBOL_NODE (node));
  g_assert (define != NULL);

  name = ide_xml_symbol_node_get_element_name (node);
  namespace = ide_xml_symbol_node_get_namespace (node);

  if (define->name != NULL && !ide_str_equal0 (name, define->name))
    return FALSE;

  if (!ide_str_empty0 ((const gchar *)define->ns))
    {
      if (namespace == NULL || !ide_str_equal0 (define->ns, namespace))
        return FALSE;
    }
  else if (namespace != NULL && (define->name != NULL || define->ns != NULL))
    return FALSE;

 if (NULL == (nc = define->name_class))
   return TRUE;

  if (nc->type == IDE_XML_RNG_DEFINE_EXCEPT)
    {
      content = nc->content;
      while (content != NULL)
        {
          if (ide_xml_rng_define_is_nameclass_match (content, node))
            return FALSE;

          content = content->next;
        }

      return TRUE;
    }
  else if (nc->type == IDE_XML_RNG_DEFINE_CHOICE)
    {
      content = define->name_class;
      while (content != NULL)
        {
          if (ide_xml_rng_define_is_nameclass_match (content, node))
            return TRUE;

          content = content->next;
        }

      return FALSE;
    }
  else
    g_return_val_if_reached (FALSE);
}
