/* ide-xml-rng-grammar.c
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

#include "ide-xml-rng-grammar.h"

G_DEFINE_BOXED_TYPE (IdeXmlRngGrammar, ide_xml_rng_grammar, ide_xml_rng_grammar_ref, ide_xml_rng_grammar_unref)

static void
dump_defines_func (const gchar *name,
                   GPtrArray   *array,
                   gpointer     data)
{
  for (guint i = 0; i < array->len; ++i)
    {
      IdeXmlRngDefine *def = g_ptr_array_index (array, i);
      ide_xml_rng_define_dump_tree (def, TRUE);
    }
}

void
ide_xml_rng_grammar_dump_tree (IdeXmlRngGrammar *self)
{
  g_return_if_fail (self != NULL);

  if (self->start_defines != NULL)
    ide_xml_rng_define_dump_tree (self->start_defines, TRUE);

  if (self->defines != NULL)
    ide_xml_hash_table_array_scan (self->defines, dump_defines_func, self);
}

IdeXmlRngGrammar *
ide_xml_rng_grammar_new (void)
{
  IdeXmlRngGrammar *self;

  self = g_slice_new0 (IdeXmlRngGrammar);
  self->ref_count = 1;

  self->defines = ide_xml_hash_table_new ((GDestroyNotify)ide_xml_rng_define_unref);
  self->refs = ide_xml_hash_table_new ((GDestroyNotify)ide_xml_rng_define_unref);

  return self;
}

void
ide_xml_rng_grammar_add_child (IdeXmlRngGrammar *self,
                               IdeXmlRngGrammar *child)
{
  IdeXmlRngGrammar *tmp_grammar;

  g_return_if_fail (self != NULL);

  if (self->children == NULL)
    self->children = child;
  else
    {
      tmp_grammar = self->children;
      while (tmp_grammar->next != NULL)
        tmp_grammar = tmp_grammar->next;

      tmp_grammar->next = child;
    }

  child->parent = self;
}

static void
ide_xml_rng_grammar_free (IdeXmlRngGrammar *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_xml_hash_table_unref (self->defines);
  ide_xml_hash_table_unref (self->refs);

  if (self->next != NULL)
    ide_xml_rng_grammar_unref (self->next);

  if (self->children != NULL)
    ide_xml_rng_grammar_unref (self->children);

  if (self->start_defines != NULL)
    ide_xml_rng_define_unref (self->start_defines);

  g_slice_free (IdeXmlRngGrammar, self);
}

IdeXmlRngGrammar *
ide_xml_rng_grammar_ref (IdeXmlRngGrammar *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_rng_grammar_unref (IdeXmlRngGrammar *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_rng_grammar_free (self);
}
