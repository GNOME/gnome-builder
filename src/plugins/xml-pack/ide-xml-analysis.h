/* ide-xml-analysis.h
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

#include <libide-code.h>

#include "ide-xml-symbol-node.h"

G_BEGIN_DECLS

#define IDE_TYPE_XML_ANALYSIS (ide_xml_analysis_get_type())

typedef struct _IdeXmlAnalysis IdeXmlAnalysis;

struct _IdeXmlAnalysis
{
  guint             ref_count;
  IdeXmlSymbolNode *root_node;
  IdeDiagnostics   *diagnostics;
  GPtrArray        *schemas;       // array of IdeXmlSchemaCacheEntry
  gint64            sequence;
};

GType               ide_xml_analysis_get_type            (void);
IdeDiagnostics     *ide_xml_analysis_get_diagnostics     (IdeXmlAnalysis   *self);
IdeXmlSymbolNode   *ide_xml_analysis_get_root_node       (IdeXmlAnalysis   *self);
gint64              ide_xml_analysis_get_sequence        (IdeXmlAnalysis   *self);
GPtrArray          *ide_xml_analysis_get_schemas         (IdeXmlAnalysis   *self);
void                ide_xml_analysis_set_diagnostics     (IdeXmlAnalysis   *self,
                                                          IdeDiagnostics   *diagnostics);
void                ide_xml_analysis_set_root_node       (IdeXmlAnalysis   *self,
                                                          IdeXmlSymbolNode *root_node);
void                ide_xml_analysis_set_sequence        (IdeXmlAnalysis   *self,
                                                          gint64            sequence);
void                ide_xml_analysis_set_schemas         (IdeXmlAnalysis   *self,
                                                          GPtrArray        *schemas);
IdeXmlAnalysis     *ide_xml_analysis_new                 (gint64            sequence);
IdeXmlAnalysis     *ide_xml_analysis_ref                 (IdeXmlAnalysis   *self);
void                ide_xml_analysis_unref               (IdeXmlAnalysis   *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeXmlAnalysis, ide_xml_analysis_unref)

G_END_DECLS
