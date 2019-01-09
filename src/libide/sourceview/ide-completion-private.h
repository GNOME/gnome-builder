/* ide-completion-private.h
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

#include "ide-completion-types.h"
#include "ide-source-view.h"

G_BEGIN_DECLS

typedef struct _IdeCompletionListBox    IdeCompletionListBox;
typedef struct _IdeCompletionListBoxRow IdeCompletionListBoxRow;
typedef struct _IdeCompletionOverlay    IdeCompletionOverlay;
typedef struct _IdeCompletionView       IdeCompletionView;
typedef struct _IdeCompletionWindow     IdeCompletionWindow;

IdeCompletionWindow     *_ide_completion_window_new                (GtkWidget                   *view);
void                     _ide_completion_view_set_font_desc        (IdeCompletionView           *self,
                                                                    const PangoFontDescription  *font_desc);
void                     _ide_completion_view_set_n_rows           (IdeCompletionView           *self,
                                                                    guint                        n_rows);
gint                     _ide_completion_view_get_x_offset         (IdeCompletionView           *self);
gboolean                 _ide_completion_view_handle_key_press     (IdeCompletionView           *self,
                                                                    const GdkEventKey           *event);
void                     _ide_completion_view_move_cursor          (IdeCompletionView           *self,
                                                                    GtkMovementStep              step,
                                                                    gint                         count);
IdeCompletion           *_ide_completion_new                       (GtkSourceView               *view);
void                     _ide_completion_set_font_description      (IdeCompletion               *self,
                                                                    const PangoFontDescription  *font_desc);
void                     _ide_completion_set_language_id           (IdeCompletion               *self,
                                                                    const gchar                 *language_id);
void                     _ide_completion_activate                  (IdeCompletion               *self,
                                                                    IdeCompletionContext        *context,
                                                                    IdeCompletionProvider       *provider,
                                                                    IdeCompletionProposal       *proposal);
IdeCompletionContext    *_ide_completion_context_new               (IdeCompletion               *completion);
gboolean                 _ide_completion_context_iter_invalidates  (IdeCompletionContext        *self,
                                                                    const GtkTextIter           *iter);
void                     _ide_completion_context_add_provider      (IdeCompletionContext        *self,
                                                                    IdeCompletionProvider       *provider);
void                     _ide_completion_context_remove_provider   (IdeCompletionContext        *self,
                                                                    IdeCompletionProvider       *provider);
gboolean                 _ide_completion_context_can_refilter      (IdeCompletionContext        *self,
                                                                    const GtkTextIter           *begin,
                                                                    const GtkTextIter           *end);
void                     _ide_completion_context_refilter          (IdeCompletionContext        *self);
void                     _ide_completion_context_complete_async    (IdeCompletionContext        *self,
                                                                    IdeCompletionActivation      activation,
                                                                    const GtkTextIter           *begin,
                                                                    const GtkTextIter           *end,
                                                                    GCancellable                *cancellable,
                                                                    GAsyncReadyCallback          callback,
                                                                    gpointer                     user_data);
gboolean                 _ide_completion_context_complete_finish   (IdeCompletionContext        *self,
                                                                    GAsyncResult                *result,
                                                                    GError                     **error);
void                     _ide_completion_display_set_font_desc     (IdeCompletionDisplay        *self,
                                                                    const PangoFontDescription  *font_desc);
gboolean                 _ide_completion_list_box_key_activates    (IdeCompletionListBox        *self,
                                                                    const GdkEventKey           *key);
void                     _ide_completion_list_box_set_font_desc    (IdeCompletionListBox        *self,
                                                                    const PangoFontDescription  *font_desc);
IdeCompletionListBoxRow *_ide_completion_list_box_get_first        (IdeCompletionListBox        *self);
void                     _ide_completion_list_box_row_attach       (IdeCompletionListBoxRow     *self,
                                                                    GtkSizeGroup                *left,
                                                                    GtkSizeGroup                *center,
                                                                    GtkSizeGroup                *right);
void                     _ide_completion_list_box_row_set_attrs    (IdeCompletionListBoxRow     *self,
                                                                    PangoAttrList               *attrs);
gint                     _ide_completion_list_box_row_get_x_offset (IdeCompletionListBoxRow     *self,
                                                                    GtkWidget                   *toplevel);
IdeCompletionOverlay    *_ide_completion_overlay_new               (void);
void                     _ide_completion_proposal_display          (IdeCompletionProposal       *self,
                                                                    IdeCompletionListBoxRow     *row);
void                     _ide_completion_provider_load             (IdeCompletionProvider       *self,
                                                                    IdeContext                  *context);

G_END_DECLS
