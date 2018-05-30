/* ide-completion-proposal.c
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

#define G_LOG_DOMAIN "ide-completion-proposal"

#include "config.h"

#include "ide-completion-proposal.h"
#include "ide-completion-provider.h"
#include "ide-completion-list-box-row.h"

G_DEFINE_INTERFACE (IdeCompletionProposal, ide_completion_proposal, G_TYPE_OBJECT)

static void
ide_completion_proposal_default_init (IdeCompletionProposalInterface *iface)
{
}

/**
 * ide_completion_proposal_get_comment:
 * @self: a #IdeCompletionProposal
 *
 * Gets the comment for the proposal, if any.
 *
 * Returns: (nullable) (transfer full): a newly allocated string or %NULL
 *
 * Since: 3.30
 */
gchar *
ide_completion_proposal_get_comment (IdeCompletionProposal *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_PROPOSAL (self), NULL);

  if (IDE_COMPLETION_PROPOSAL_GET_IFACE (self)->get_comment)
    return IDE_COMPLETION_PROPOSAL_GET_IFACE (self)->get_comment (self);

  return NULL;
}
