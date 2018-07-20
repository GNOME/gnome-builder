/* ide-xml-detail.h
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_XML_DETAIL (ide_xml_detail_get_type())

typedef struct _IdeXmlDetail IdeXmlDetail;

typedef enum
{
  IDE_XML_DETAIL_MEMBER_NONE,
  IDE_XML_DETAIL_MEMBER_NAME,
  IDE_XML_DETAIL_MEMBER_ATTRIBUTE_NAME,
  IDE_XML_DETAIL_MEMBER_ATTRIBUTE_VALUE
} IdeXmlDetailMember;

typedef enum
{
  IDE_XML_DETAIL_SIDE_NONE,
  IDE_XML_DETAIL_SIDE_LEFT,
  IDE_XML_DETAIL_SIDE_MIDDLE,
  IDE_XML_DETAIL_SIDE_RIGHT
} IdeXmlDetailSide;

struct _IdeXmlDetail
{
  volatile gint       ref_count;
  gchar              *name;
  gchar              *value;
  gchar              *prefix;
  IdeXmlDetailMember  member;
  IdeXmlDetailSide    side;
  gchar               quote;
};

GType         ide_xml_detail_get_type (void);
IdeXmlDetail *ide_xml_detail_new      (const gchar        *name,
                                       const gchar        *value,
                                       const gchar        *prefix,
                                       IdeXmlDetailMember  member,
                                       IdeXmlDetailSide    side,
                                       gchar               quote);
void          ide_xml_detail_print    (IdeXmlDetail       *self);
IdeXmlDetail *ide_xml_detail_ref      (IdeXmlDetail       *self);
void          ide_xml_detail_unref    (IdeXmlDetail       *self);


G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeXmlDetail, ide_xml_detail_unref)

G_END_DECLS
