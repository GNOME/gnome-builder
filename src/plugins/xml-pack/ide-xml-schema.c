/* ide-xml-schema.c
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

#include "ide-xml-schema.h"

G_DEFINE_BOXED_TYPE (IdeXmlSchema, ide_xml_schema, ide_xml_schema_ref, ide_xml_schema_unref)

IdeXmlSchema *
ide_xml_schema_new (void)
{
  IdeXmlSchema *self;

  self = g_slice_new0 (IdeXmlSchema);
  self->ref_count = 1;

  return self;
}

IdeXmlSchema *
ide_xml_schema_copy (IdeXmlSchema *self)
{
  IdeXmlSchema *copy;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  copy = ide_xml_schema_new ();

  return copy;
}

static void
ide_xml_schema_free (IdeXmlSchema *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  if (self->top_grammar != NULL)
    ide_xml_rng_grammar_unref (self->top_grammar);

  g_slice_free (IdeXmlSchema, self);
}

IdeXmlSchema *
ide_xml_schema_ref (IdeXmlSchema *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_schema_unref (IdeXmlSchema *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_schema_free (self);
}
