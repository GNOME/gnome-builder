/* ide-xml-position.c
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

#include "ide-xml-position.h"

G_DEFINE_BOXED_TYPE (IdeXmlPosition, ide_xml_position, ide_xml_position_ref, ide_xml_position_unref)

IdeXmlPosition *
ide_xml_position_new (void)
{
  IdeXmlPosition *self;

  self = g_slice_new0 (IdeXmlPosition);
  self->ref_count = 1;

  return self;
}

IdeXmlPosition *
ide_xml_position_copy (IdeXmlPosition *self)
{
  IdeXmlPosition *copy;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  copy = ide_xml_position_new ();

  return copy;
}

static void
ide_xml_position_free (IdeXmlPosition *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  g_slice_free (IdeXmlPosition, self);
}

IdeXmlPosition *
ide_xml_position_ref (IdeXmlPosition *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_position_unref (IdeXmlPosition *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_position_free (self);
}
