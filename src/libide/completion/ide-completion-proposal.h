/* ide-completion-proposal.h
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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

#include <glib-object.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_PROPOSAL (ide_completion_proposal_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_INTERFACE (IdeCompletionProposal, ide_completion_proposal, IDE, COMPLETION_PROPOSAL, GObject)

struct _IdeCompletionProposalInterface
{
  GTypeInterface parent_iface;
};

G_END_DECLS
