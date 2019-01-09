/* ide-clang-proposals.h
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

#include <libide-code.h>

#include "ide-clang-client.h"

G_BEGIN_DECLS

#define IDE_TYPE_CLANG_PROPOSALS (ide_clang_proposals_get_type())

G_DECLARE_FINAL_TYPE (IdeClangProposals, ide_clang_proposals, IDE, CLANG_PROPOSALS, GObject)

IdeClangProposals *ide_clang_proposals_new             (IdeClangClient       *client);
IdeClangClient    *ide_clang_proposals_get_client      (IdeClangProposals    *self);
void               ide_clang_proposals_clear           (IdeClangProposals    *self);
void               ide_clang_proposals_refilter        (IdeClangProposals    *self,
                                                        const gchar          *word);
void               ide_clang_proposals_populate_async  (IdeClangProposals    *self,
                                                        const GtkTextIter    *iter,
                                                        const gchar          *word,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
gboolean           ide_clang_proposals_populate_finish (IdeClangProposals    *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);

G_END_DECLS
