/* ide-docs-search-view.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include "ide-docs-item.h"
#include "ide-docs-query.h"
#include "ide-docs-search-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_DOCS_SEARCH_VIEW (ide_docs_search_view_get_type())

G_DECLARE_FINAL_TYPE (IdeDocsSearchView, ide_docs_search_view, IDE, DOCS_SEARCH_VIEW, GtkBin)

GtkWidget *ide_docs_search_view_new           (void);
void       ide_docs_search_view_add_sections  (IdeDocsSearchView    *self,
                                               IdeDocsItem          *parent);
void       ide_docs_search_view_search_async  (IdeDocsSearchView    *self,
                                               IdeDocsQuery         *query,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
gboolean   ide_docs_search_view_search_finish (IdeDocsSearchView    *self,
                                               GAsyncResult         *result,
                                               GError              **error);

G_END_DECLS
