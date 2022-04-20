/* gbp-pygi-proposal.c
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

#define G_LOG_DOMAIN "gbp-pygi-proposal"

#include "config.h"

#include "gbp-pygi-proposal.h"

struct _GbpPygiProposal
{
  GObject parent_instance;
  const char *name;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpPygiProposal, gbp_pygi_proposal, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, NULL))

static void
gbp_pygi_proposal_class_init (GbpPygiProposalClass *klass)
{
}

static void
gbp_pygi_proposal_init (GbpPygiProposal *self)
{
}

GbpPygiProposal *
gbp_pygi_proposal_new (const char *name)
{
  GbpPygiProposal *self;

  self = g_object_new (GBP_TYPE_PYGI_PROPOSAL, NULL);
  self->name = g_intern_string (name);

  return self;
}

void
gbp_pygi_proposal_display (GbpPygiProposal            *self,
                           GtkSourceCompletionContext *context,
                           GtkSourceCompletionCell    *cell)
{
  GtkSourceCompletionColumn column;

  g_assert (GBP_IS_PYGI_PROPOSAL (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CELL (cell));

  column = gtk_source_completion_cell_get_column (cell);

  if (column == GTK_SOURCE_COMPLETION_COLUMN_ICON)
    {
      gtk_source_completion_cell_set_icon_name (cell, "lang-namespace-symbolic");
    }
  else if (column == GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT)
    {
      g_autofree char *typed_text = gtk_source_completion_context_get_word (context);
      g_autoptr(PangoAttrList) attrs = gtk_source_completion_fuzzy_highlight (self->name, typed_text);

      gtk_source_completion_cell_set_text_with_attributes (cell, self->name, attrs);
    }
  else
    {
      gtk_source_completion_cell_set_text (cell, NULL);
    }
}

const char *
gbp_pygi_proposal_get_name (GbpPygiProposal *self)
{
  g_return_val_if_fail (GBP_IS_PYGI_PROPOSAL (self), NULL);

  return self->name;
}
