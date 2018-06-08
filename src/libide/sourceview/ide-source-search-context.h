/* ide-source-search-context.h
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

#include <gtksourceview/gtksource.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

/*
 * This is a workaround for a GtkSourceView bug that allows us to maintain
 * a compatible API to upgrade code easliy once it is fixed.
 *
 * https://gitlab.gnome.org/GNOME/gtksourceview/issues/8
 */

IDE_AVAILABLE_IN_3_30
void     ide_source_search_context_backward_async   (GtkSourceSearchContext  *search,
                                                     const GtkTextIter       *iter,
                                                     GCancellable            *cancellable,
                                                     GAsyncReadyCallback      callback,
                                                     gpointer                 user_data);
IDE_AVAILABLE_IN_3_30
gboolean ide_source_search_context_backward_finish2 (GtkSourceSearchContext  *search,
                                                     GAsyncResult            *result,
                                                     GtkTextIter             *match_begin,
                                                     GtkTextIter             *match_end,
                                                     gboolean                *has_wrapped_around,
                                                     GError                 **error);


G_END_DECLS
