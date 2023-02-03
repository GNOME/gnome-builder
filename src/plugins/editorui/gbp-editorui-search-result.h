/* gbp-editorui-search-result.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include <libide-search.h>

G_BEGIN_DECLS

#define GBP_TYPE_EDITORUI_SEARCH_RESULT (gbp_editorui_search_result_get_type())

G_DECLARE_FINAL_TYPE (GbpEditoruiSearchResult, gbp_editorui_search_result, GBP, EDITORUI_SEARCH_RESULT, IdeSearchResult)

IdeSearchResult *gbp_editorui_search_result_new (GtkSourceStyleScheme *scheme);

G_END_DECLS
