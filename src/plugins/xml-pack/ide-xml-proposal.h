/* ide-xml-proposal.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_XML_PROPOSAL (ide_xml_proposal_get_type())

G_DECLARE_FINAL_TYPE (IdeXmlProposal, ide_xml_proposal, IDE, XML_PROPOSAL, GObject)

IdeXmlProposal *ide_xml_proposal_new       (const gchar    *text,
                                            const gchar    *label);
const gchar    *ide_xml_proposal_get_label (IdeXmlProposal *self);
const gchar    *ide_xml_proposal_get_text  (IdeXmlProposal *self);

G_END_DECLS
