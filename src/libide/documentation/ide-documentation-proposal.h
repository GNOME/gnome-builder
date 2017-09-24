/* ide-documentation-proposal.h
 *
 * Copyright (C) 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_DOCUMENTATION_PROPOSAL            (ide_documentation_proposal_get_type())

struct _IdeDocumentationProposalClass
{
  GObjectClass parent_class;
};

typedef struct _IdeDocumentationProposalClass IdeDocumentationProposalClass;

G_DECLARE_DERIVABLE_TYPE (IdeDocumentationProposal, ide_documentation_proposal, IDE, DOCUMENTATION_PROPOSAL, GObject)

IdeDocumentationProposal *ide_documentation_proposal_new            (const gchar                *url);
void                      ide_documentation_proposal_set_header     (IdeDocumentationProposal   *self,
                                                                     const gchar                *header);
void                      ide_documentation_proposal_set_text       (IdeDocumentationProposal   *self,
                                                                     const gchar                *text);
const gchar              *ide_documentation_proposal_get_header     (IdeDocumentationProposal   *self);
const gchar              *ide_documentation_proposal_get_text       (IdeDocumentationProposal   *self);
const gchar              *ide_documentation_proposal_get_uri        (IdeDocumentationProposal   *self);


G_END_DECLS
