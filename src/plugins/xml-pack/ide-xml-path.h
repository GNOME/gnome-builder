/* ide-xml-path.h
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

#include "ide-xml-symbol-node.h"

G_BEGIN_DECLS

#define IDE_TYPE_XML_PATH (ide_xml_path_get_type())

typedef struct _IdeXmlPath IdeXmlPath;

struct _IdeXmlPath
{
  volatile gint  ref_count;
  GPtrArray     *nodes;
  guint          start_at_root : 1;
};

GType       ide_xml_path_get_type      (void);
IdeXmlPath *ide_xml_path_new           (void);
IdeXmlPath *ide_xml_path_new_from_node (IdeXmlSymbolNode *node);
IdeXmlPath *ide_xml_path_copy          (IdeXmlPath       *self);
IdeXmlPath *ide_xml_path_ref           (IdeXmlPath       *self);
void        ide_xml_path_unref         (IdeXmlPath       *self);
void        ide_xml_path_append_node   (IdeXmlPath       *self,
                                        IdeXmlSymbolNode *node);
void        ide_xml_path_dump          (IdeXmlPath       *self);
void        ide_xml_path_prepend_node  (IdeXmlPath       *self,
                                        IdeXmlSymbolNode *node);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeXmlPath, ide_xml_path_unref)

G_END_DECLS
