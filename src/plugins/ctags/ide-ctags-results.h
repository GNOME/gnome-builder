/* ide-ctags-results.h
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

#include "ide-ctags-index.h"

G_BEGIN_DECLS

#define IDE_TYPE_CTAGS_RESULTS (ide_ctags_results_get_type())

G_DECLARE_FINAL_TYPE (IdeCtagsResults, ide_ctags_results, IDE, CTAGS_RESULTS, GObject)

IdeCtagsResults *ide_ctags_results_new             (void);
void             ide_ctags_results_set_suffixes    (IdeCtagsResults      *self,
                                                    const gchar * const  *suffixes);
void             ide_ctags_results_add_index       (IdeCtagsResults      *self,
                                                    IdeCtagsIndex        *index);
void             ide_ctags_results_set_word        (IdeCtagsResults      *self,
                                                    const gchar          *word);
void             ide_ctags_results_refilter        (IdeCtagsResults      *self);
void             ide_ctags_results_populate_async  (IdeCtagsResults      *self,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
gboolean         ide_ctags_results_populate_finish (IdeCtagsResults      *self,
                                                    GAsyncResult         *result,
                                                    GError              **error);

G_END_DECLS
