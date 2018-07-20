/* ide-xml-detail.c
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#include <dazzle.h>

#include "ide-xml-detail.h"

G_DEFINE_BOXED_TYPE (IdeXmlDetail, ide_xml_detail, ide_xml_detail_ref, ide_xml_detail_unref)

IdeXmlDetail *
ide_xml_detail_new (const gchar        *name,
                    const gchar        *value,
                    const gchar        *prefix,
                    IdeXmlDetailMember  member,
                    IdeXmlDetailSide    side,
                    gchar               quote)
{
  IdeXmlDetail *self;

  self = g_slice_new0 (IdeXmlDetail);
  self->ref_count = 1;

  self->name = g_strdup (name);
  self->value = g_strdup (value);
  self->prefix = g_strdup (prefix);
  self->member = member;
  self->side = side;
  self->quote = quote;

  return self;
}

static void
ide_xml_detail_free (IdeXmlDetail *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  dzl_clear_pointer (&self->name, g_free);
  dzl_clear_pointer (&self->value, g_free);
  dzl_clear_pointer (&self->prefix, g_free);

  g_slice_free (IdeXmlDetail, self);
}

IdeXmlDetail *
ide_xml_detail_ref (IdeXmlDetail *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_detail_unref (IdeXmlDetail *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_detail_free (self);
}

void
ide_xml_detail_print (IdeXmlDetail *self)
{
  const gchar *member;
  const gchar *side;

  switch (self->member)
    {
    case IDE_XML_DETAIL_MEMBER_NONE:
      member = "none";
      break;

    case IDE_XML_DETAIL_MEMBER_NAME:
      member = "name";
      break;

    case IDE_XML_DETAIL_MEMBER_ATTRIBUTE_NAME:
      member = "attribute name";
      break;

    case IDE_XML_DETAIL_MEMBER_ATTRIBUTE_VALUE:
      member = "attribute value";
      break;

    default:
      g_assert_not_reached ();
    }

  switch (self->side)
    {
    case IDE_XML_DETAIL_SIDE_NONE:
      side = "none";
      break;

    case IDE_XML_DETAIL_SIDE_LEFT:
      side = "left";
      break;

    case IDE_XML_DETAIL_SIDE_MIDDLE:
      side = "middle";
      break;

    case IDE_XML_DETAIL_SIDE_RIGHT:
      side = "right";
      break;

    default:
      g_assert_not_reached ();
    }

  g_print ("name:'%s' value:'%s' prefix:'%s' member:'%s' side:'%s' quote:%c\n",
           self->name,
           self->value,
           self->prefix,
           member,
           side,
           self->quote);
}
