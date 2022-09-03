/* gbp-top-match-completion-proposal.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-top-match-completion-proposal"

#include "config.h"

#include "gbp-top-match-completion-proposal.h"

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTopMatchCompletionProposal, gbp_top_match_completion_proposal, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
gbp_top_match_completion_proposal_dispose (GObject *object)
{
  GbpTopMatchCompletionProposal *self = (GbpTopMatchCompletionProposal *)object;

  g_clear_object (&self->provider);
  g_clear_object (&self->proposal);

  G_OBJECT_CLASS (gbp_top_match_completion_proposal_parent_class)->dispose (object);
}

static void
gbp_top_match_completion_proposal_class_init (GbpTopMatchCompletionProposalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_top_match_completion_proposal_dispose;
}

static void
gbp_top_match_completion_proposal_init (GbpTopMatchCompletionProposal *self)
{
}

GbpTopMatchCompletionProposal *
gbp_top_match_completion_proposal_new (GtkSourceCompletionProvider *provider,
                                       GtkSourceCompletionProposal *proposal)
{
  GbpTopMatchCompletionProposal *self;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider), NULL);
  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal), NULL);

  self = g_object_new (GBP_TYPE_TOP_MATCH_COMPLETION_PROPOSAL, NULL);
  g_set_object (&self->provider, provider);
  g_set_object (&self->proposal, proposal);

  return self;
}
