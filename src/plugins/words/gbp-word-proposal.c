/* gbp-word-proposal.c
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

#include "config.h"

#define G_LOG_DOMAIN "gbp-word-proposal"

#include <ide.h>

#include "gbp-word-proposal.h"

struct _GbpWordProposal
{
  GObject parent_instance;
  gchar *word;
};

G_DEFINE_TYPE_WITH_CODE (GbpWordProposal, gbp_word_proposal, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROPOSAL, NULL))

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

const gchar *
gbp_word_proposal_get_word (GbpWordProposal *self)
{
  g_return_val_if_fail (GBP_IS_WORD_PROPOSAL (self), NULL);

  return self->word;
}
