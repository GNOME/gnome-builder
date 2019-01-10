/* ide-completion-context.h
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

#include "ide-completion-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_CONTEXT (ide_completion_context_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeCompletionContext, ide_completion_context, IDE, COMPLETION_CONTEXT, GObject)

IDE_AVAILABLE_IN_3_32
IdeCompletionActivation  ide_completion_context_get_activation             (IdeCompletionContext   *self);
IDE_AVAILABLE_IN_3_32
const gchar             *ide_completion_context_get_language               (IdeCompletionContext   *self);
IDE_AVAILABLE_IN_3_32
gboolean                 ide_completion_context_is_language                (IdeCompletionContext   *self,
                                                                            const gchar            *language);
IDE_AVAILABLE_IN_3_32
GtkTextBuffer           *ide_completion_context_get_buffer                 (IdeCompletionContext   *self);
IDE_AVAILABLE_IN_3_32
GtkTextView             *ide_completion_context_get_view                   (IdeCompletionContext   *self);
IDE_AVAILABLE_IN_3_32
gboolean                 ide_completion_context_get_busy                   (IdeCompletionContext   *self);
IDE_AVAILABLE_IN_3_32
gboolean                 ide_completion_context_is_empty                   (IdeCompletionContext   *self);
IDE_AVAILABLE_IN_3_32
void                     ide_completion_context_set_proposals_for_provider (IdeCompletionContext   *self,
                                                                            IdeCompletionProvider  *provider,
                                                                            GListModel             *results);
IDE_AVAILABLE_IN_3_32
IdeCompletion           *ide_completion_context_get_completion             (IdeCompletionContext   *self);
IDE_AVAILABLE_IN_3_32
gboolean                 ide_completion_context_get_bounds                 (IdeCompletionContext   *self,
                                                                            GtkTextIter            *begin,
                                                                            GtkTextIter            *end);
IDE_AVAILABLE_IN_3_32
gboolean                 ide_completion_context_get_start_iter             (IdeCompletionContext   *self,
                                                                            GtkTextIter            *iter);
IDE_AVAILABLE_IN_3_32
gchar                   *ide_completion_context_get_word                   (IdeCompletionContext   *self);
IDE_AVAILABLE_IN_3_32
gchar                   *ide_completion_context_get_line_text              (IdeCompletionContext   *self);
IDE_AVAILABLE_IN_3_32
gboolean                 ide_completion_context_get_item_full              (IdeCompletionContext   *self,
                                                                            guint                   position,
                                                                            IdeCompletionProvider **provider,
                                                                            IdeCompletionProposal **proposal);

G_END_DECLS
