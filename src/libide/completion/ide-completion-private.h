/* ide-completion-private.h
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

#include "completion/ide-completion-types.h"
#include "sourceview/ide-source-view.h"

G_BEGIN_DECLS

IdeCompletionWindow     *_ide_completion_window_new                (GtkWidget                *view);
void                     _ide_completion_view_set_n_rows           (IdeCompletionView        *self,
                                                                    guint                     n_rows);
gint                     _ide_completion_view_get_x_offset         (IdeCompletionView        *self);
gboolean                 _ide_completion_view_handle_key_press     (IdeCompletionView        *self,
                                                                    const GdkEventKey        *event);
IdeCompletion           *_ide_completion_new                       (GtkSourceView            *view);
void                     _ide_completion_activate                  (IdeCompletion            *self,
                                                                    IdeCompletionContext     *context,
                                                                    IdeCompletionProvider    *provider,
                                                                    IdeCompletionProposal    *proposal);
IdeCompletionContext    *_ide_completion_context_new               (IdeCompletion            *completion);
void                     _ide_completion_context_add_provider      (IdeCompletionContext     *self,
                                                                    IdeCompletionProvider    *provider);
void                     _ide_completion_context_remove_provider   (IdeCompletionContext     *self,
                                                                    IdeCompletionProvider    *provider);
gboolean                 _ide_completion_context_can_refilter      (IdeCompletionContext     *self,
                                                                    const GtkTextIter        *begin,
                                                                    const GtkTextIter        *end);
void                     _ide_completion_context_refilter          (IdeCompletionContext     *self);
void                     _ide_completion_context_complete_async    (IdeCompletionContext     *self,
                                                                    const GtkTextIter        *begin,
                                                                    const GtkTextIter        *end,
                                                                    GCancellable             *cancellable,
                                                                    GAsyncReadyCallback       callback,
                                                                    gpointer                  user_data);
gboolean                 _ide_completion_context_complete_finish   (IdeCompletionContext     *self,
                                                                    GAsyncResult             *result,
                                                                    GError                  **error);
gboolean                 _ide_completion_context_get_proposal      (IdeCompletionContext     *self,
                                                                    guint                     position,
                                                                    IdeCompletionProvider   **provider,
                                                                    IdeCompletionProposal   **proposal);
gboolean                 _ide_completion_list_box_key_activates    (IdeCompletionListBox     *self,
                                                                    const GdkEventKey        *key);
IdeCompletionListBoxRow *_ide_completion_list_box_get_first        (IdeCompletionListBox     *self);
void                     _ide_completion_list_box_row_attach       (IdeCompletionListBoxRow  *self,
                                                                    GtkSizeGroup             *left,
                                                                    GtkSizeGroup             *center,
                                                                    GtkSizeGroup             *right);
gint                     _ide_completion_list_box_row_get_x_offset (IdeCompletionListBoxRow  *self,
                                                                    GtkWidget                *toplevel);
IdeCompletionOverlay    *_ide_completion_overlay_new               (void);

G_END_DECLS
