/* gbp-word-proposals.h
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

#pragma once

#include <libide-sourceview.h>

G_BEGIN_DECLS

#define GBP_TYPE_WORD_PROPOSALS (gbp_word_proposals_get_type())

G_DECLARE_FINAL_TYPE (GbpWordProposals, gbp_word_proposals, GBP, WORD_PROPOSALS, GObject)

GbpWordProposals *gbp_word_proposals_new             (void);
void              gbp_word_proposals_populate_async  (GbpWordProposals            *self,
                                                      GtkSourceCompletionContext  *context,
                                                      GCancellable                *cancellable,
                                                      GAsyncReadyCallback          callback,
                                                      gpointer                     user_data);
gboolean          gbp_word_proposals_populate_finish (GbpWordProposals            *self,
                                                      GAsyncResult                *result,
                                                      GError                     **error);
void              gbp_word_proposals_refilter        (GbpWordProposals            *self,
                                                      const gchar                 *word);
void              gbp_word_proposals_clear           (GbpWordProposals            *self);

G_END_DECLS
