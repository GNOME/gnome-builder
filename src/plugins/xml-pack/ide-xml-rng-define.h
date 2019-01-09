/* ide-xml-rng-define.h
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

#pragma once

#include <glib-object.h>
#include <libxml/tree.h>

#include "ide-xml-symbol-node.h"

G_BEGIN_DECLS

#define IDE_TYPE_XML_RNG_DEFINE (ide_xml_rng_define_get_type())

typedef struct _IdeXmlRngDefine IdeXmlRngDefine;

typedef enum
{
  IDE_XML_RNG_DEFINE_NOOP,
  IDE_XML_RNG_DEFINE_DEFINE,
  IDE_XML_RNG_DEFINE_EMPTY,
  IDE_XML_RNG_DEFINE_NOTALLOWED,
  IDE_XML_RNG_DEFINE_TEXT,
  IDE_XML_RNG_DEFINE_ELEMENT,
  IDE_XML_RNG_DEFINE_DATATYPE,
  IDE_XML_RNG_DEFINE_VALUE,
  IDE_XML_RNG_DEFINE_LIST,
  IDE_XML_RNG_DEFINE_REF,
  IDE_XML_RNG_DEFINE_PARENTREF,
  IDE_XML_RNG_DEFINE_EXTERNALREF,
  IDE_XML_RNG_DEFINE_ZEROORMORE,
  IDE_XML_RNG_DEFINE_ONEORMORE,
  IDE_XML_RNG_DEFINE_OPTIONAL,
  IDE_XML_RNG_DEFINE_CHOICE,
  IDE_XML_RNG_DEFINE_GROUP,
  IDE_XML_RNG_DEFINE_ATTRIBUTES_GROUP,
  IDE_XML_RNG_DEFINE_INTERLEAVE,
  IDE_XML_RNG_DEFINE_ATTRIBUTE,
  IDE_XML_RNG_DEFINE_START,
  IDE_XML_RNG_DEFINE_PARAM,
  IDE_XML_RNG_DEFINE_EXCEPT
} IdeXmlRngDefineType;

struct _IdeXmlRngDefine
{
  volatile gint        ref_count;

  xmlChar             *name;
  xmlChar             *ns;
  IdeXmlRngDefine     *parent;
  IdeXmlRngDefine     *next;
  IdeXmlRngDefine     *content;
  IdeXmlRngDefine     *attributes;
  IdeXmlRngDefine     *name_class;
  xmlNode             *node;
  IdeXmlRngDefineType  type;

  gint16               depth;
  gint                 pos;

  guint                is_external_ref : 1;
  guint                is_ref_simplified : 1;

  /* This field is relevant only for the current completion */
  guint                is_mandatory : 1;
};

GType            ide_xml_rng_define_get_type           (void);
IdeXmlRngDefine *ide_xml_rng_define_new                (xmlNode             *node,
                                                        IdeXmlRngDefine     *parent,
                                                        const guchar        *name,
                                                        IdeXmlRngDefineType  type);
void             ide_xml_rng_define_append             (IdeXmlRngDefine     *self,
                                                        IdeXmlRngDefine     *def);
const gchar     *ide_xml_rng_define_get_type_name      (IdeXmlRngDefine     *self);
gboolean         ide_xml_rng_define_is_nameclass_match (IdeXmlRngDefine     *define,
                                                        IdeXmlSymbolNode    *node);
void             ide_xml_rng_define_propagate_parent   (IdeXmlRngDefine     *self,
                                                        IdeXmlRngDefine     *parent);
IdeXmlRngDefine *ide_xml_rng_define_ref                (IdeXmlRngDefine     *self);
void             ide_xml_rng_define_unref              (IdeXmlRngDefine     *self);
void             ide_xml_rng_define_dump_tree          (IdeXmlRngDefine     *self,
                                                        gboolean             recursive);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeXmlRngDefine, ide_xml_rng_define_unref)

G_END_DECLS
