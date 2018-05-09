/* ide-completion-compat.h
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

#include <gtksourceview/gtksource.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

IDE_AVAILABLE_IN_ALL
gboolean  ide_completion_provider_context_in_comment           (GtkSourceCompletionContext *context);
IDE_AVAILABLE_IN_ALL
gboolean  ide_completion_provider_context_in_comment_or_string (GtkSourceCompletionContext *context);
IDE_AVAILABLE_IN_ALL
gchar    *ide_completion_provider_context_current_word         (GtkSourceCompletionContext *context);

G_END_DECLS
