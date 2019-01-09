/* ide-completion-window.h
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

#if !defined (IDE_SOURCEVIEW_INSIDE) && !defined (IDE_SOURCEVIEW_COMPILATION)
# error "Only <libide-sourceview.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_WINDOW (ide_completion_window_get_type())

G_DECLARE_FINAL_TYPE (IdeCompletionWindow, ide_completion_window, IDE, COMPLETION_WINDOW, GtkWindow)

IdeCompletionContext *ide_completion_window_get_context (IdeCompletionWindow  *self);
void                  ide_completion_window_set_context (IdeCompletionWindow  *self,
                                                         IdeCompletionContext *context);

G_END_DECLS
