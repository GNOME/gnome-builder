/* gbp-pygi-proposal.h
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

#pragma once

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GBP_TYPE_PYGI_PROPOSAL (gbp_pygi_proposal_get_type())

G_DECLARE_FINAL_TYPE (GbpPygiProposal, gbp_pygi_proposal, GBP, PYGI_PROPOSAL, GObject)

GbpPygiProposal *gbp_pygi_proposal_new      (const char                 *name);
const char      *gbp_pygi_proposal_get_name (GbpPygiProposal            *self);
void             gbp_pygi_proposal_display  (GbpPygiProposal            *self,
                                             GtkSourceCompletionContext *context,
                                             GtkSourceCompletionCell    *cell);

G_END_DECLS
