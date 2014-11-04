/* gb-source-auto-indenter-xml.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GB_SOURCE_AUTO_INDENTER_XML_H
#define GB_SOURCE_AUTO_INDENTER_XML_H

#include "gb-source-auto-indenter.h"

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_AUTO_INDENTER_XML            (gb_source_auto_indenter_xml_get_type())
#define GB_SOURCE_AUTO_INDENTER_XML(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_AUTO_INDENTER_XML, GbSourceAutoIndenterXml))
#define GB_SOURCE_AUTO_INDENTER_XML_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_AUTO_INDENTER_XML, GbSourceAutoIndenterXml const))
#define GB_SOURCE_AUTO_INDENTER_XML_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_AUTO_INDENTER_XML, GbSourceAutoIndenterXmlClass))
#define GB_IS_SOURCE_AUTO_INDENTER_XML(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_AUTO_INDENTER_XML))
#define GB_IS_SOURCE_AUTO_INDENTER_XML_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_AUTO_INDENTER_XML))
#define GB_SOURCE_AUTO_INDENTER_XML_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_AUTO_INDENTER_XML, GbSourceAutoIndenterXmlClass))

typedef struct _GbSourceAutoIndenterXml        GbSourceAutoIndenterXml;
typedef struct _GbSourceAutoIndenterXmlClass   GbSourceAutoIndenterXmlClass;
typedef struct _GbSourceAutoIndenterXmlPrivate GbSourceAutoIndenterXmlPrivate;

struct _GbSourceAutoIndenterXml
{
  GbSourceAutoIndenter parent;

  /*< private >*/
  GbSourceAutoIndenterXmlPrivate *priv;
};

struct _GbSourceAutoIndenterXmlClass
{
  GbSourceAutoIndenterClass parent;
};

GType                 gb_source_auto_indenter_xml_get_type (void);
GbSourceAutoIndenter *gb_source_auto_indenter_xml_new      (void);

G_END_DECLS

#endif /* GB_SOURCE_AUTO_INDENTER_XML_H */
