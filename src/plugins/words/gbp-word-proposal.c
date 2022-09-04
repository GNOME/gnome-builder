/* gbp-word-proposal.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-word-proposal"

#include "config.h"

#include <libide-sourceview.h>

#include "gbp-word-proposal.h"

struct _GbpWordProposal
{
  GObject parent_instance;
  char *word;
};

static char *
gbp_word_proposal_get_typed_text (GtkSourceCompletionProposal *proposal)
{
  return g_strdup (GBP_WORD_PROPOSAL (proposal)->word);
}

static void
proposal_iface_init (GtkSourceCompletionProposalInterface *iface)
{
  iface->get_typed_text = gbp_word_proposal_get_typed_text;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpWordProposal, gbp_word_proposal, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, proposal_iface_init))

static void
gbp_word_proposal_finalize (GObject *object)
{
  GbpWordProposal *self = (GbpWordProposal *)object;

  g_clear_pointer (&self->word, g_free);

  G_OBJECT_CLASS (gbp_word_proposal_parent_class)->finalize (object);
}

static void
gbp_word_proposal_class_init (GbpWordProposalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_word_proposal_finalize;
}

static void
gbp_word_proposal_init (GbpWordProposal *self)
{
}

GbpWordProposal *
gbp_word_proposal_new (const gchar *word)
{
  GbpWordProposal *self;

  self = g_object_new (GBP_TYPE_WORD_PROPOSAL, NULL);
  self->word = g_strdup (word);

  return self;
}

const char *
gbp_word_proposal_get_word (GbpWordProposal *self)
{
  g_return_val_if_fail (GBP_IS_WORD_PROPOSAL (self), NULL);

  return self->word;
}
