/* ide-docs-search-row.h
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

#include <dazzle.h>

#include "ide-docs-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_DOCS_SEARCH_ROW (ide_docs_search_row_get_type())

G_DECLARE_FINAL_TYPE (IdeDocsSearchRow, ide_docs_search_row, IDE, DOCS_SEARCH_ROW, DzlListBoxRow)

GtkWidget   *ide_docs_search_row_new      (IdeDocsItem      *item);
IdeDocsItem *ide_docs_search_row_get_item (IdeDocsSearchRow *self);

G_END_DECLS
