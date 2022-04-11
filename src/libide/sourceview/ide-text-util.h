/* ide-text-util.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#if !defined (IDE_SOURCEVIEW_INSIDE) && !defined (IDE_SOURCEVIEW_COMPILATION)
# error "Only <libide-sourceview.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

IDE_AVAILABLE_IN_ALL
void ide_text_util_delete_line          (GtkTextView  *text_view,
                                         int           count);
IDE_AVAILABLE_IN_ALL
void ide_text_util_remove_common_prefix (GtkTextIter  *begin,
                                         const char   *prefix);
IDE_AVAILABLE_IN_ALL
int  ide_text_util_int_to_string        (guint         value,
                                         const char  **outstr);

G_END_DECLS
