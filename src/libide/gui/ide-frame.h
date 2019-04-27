/* ide-frame.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <libide-core.h>

#include "ide-page.h"

G_BEGIN_DECLS

#define IDE_TYPE_FRAME (ide_frame_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_DERIVABLE_TYPE (IdeFrame, ide_frame, IDE, FRAME, GtkBox)

struct _IdeFrameClass
{
  GtkBoxClass parent_class;

  void     (*agree_to_close_async)  (IdeFrame             *stack,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data);
  gboolean (*agree_to_close_finish) (IdeFrame             *stack,
                                     GAsyncResult         *result,
                                     GError              **error);

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_3_32
GtkWidget *ide_frame_new                   (void);
IDE_AVAILABLE_IN_3_32
GtkWidget *ide_frame_get_titlebar          (IdeFrame             *self);
IDE_AVAILABLE_IN_3_32
IdePage   *ide_frame_get_visible_child     (IdeFrame             *self);
IDE_AVAILABLE_IN_3_32
void       ide_frame_set_visible_child     (IdeFrame             *self,
                                            IdePage              *page);
IDE_AVAILABLE_IN_3_32
gboolean   ide_frame_get_has_page          (IdeFrame             *self);
IDE_AVAILABLE_IN_3_32
void       ide_frame_agree_to_close_async  (IdeFrame             *self,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean   ide_frame_agree_to_close_finish (IdeFrame             *self,
                                            GAsyncResult         *result,
                                            GError              **error);
IDE_AVAILABLE_IN_3_32
void       ide_frame_foreach_page          (IdeFrame             *self,
                                            GtkCallback           callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_3_32
void       ide_frame_add_with_depth        (IdeFrame             *self,
                                            GtkWidget            *widget,
                                            guint                 position);
IDE_AVAILABLE_IN_3_34
void       ide_frame_set_placeholder       (IdeFrame             *self,
                                            GtkWidget            *placeholder);

G_END_DECLS
