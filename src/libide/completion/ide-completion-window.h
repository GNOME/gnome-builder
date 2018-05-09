/* ide-completion-window.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_WINDOW (ide_completion_window_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeCompletionWindow, ide_completion_window, IDE, COMPLETION_WINDOW, GtkWindow)

IDE_AVAILABLE_IN_3_30
IdeCompletionContext *ide_completion_window_get_context (IdeCompletionWindow  *self);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_window_set_context (IdeCompletionWindow  *self,
                                                         IdeCompletionContext *context);

G_END_DECLS
