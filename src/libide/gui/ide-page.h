/* ide-page.h
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

G_BEGIN_DECLS

#define IDE_TYPE_PAGE (ide_page_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_DERIVABLE_TYPE (IdePage, ide_page, IDE, PAGE, GtkBox)

struct _IdePageClass
{
  GtkBoxClass parent_class;

  void           (*agree_to_close_async)  (IdePage              *self,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data);
  gboolean       (*agree_to_close_finish) (IdePage              *self,
                                           GAsyncResult         *result,
                                           GError              **error);
  IdePage       *(*create_split)          (IdePage              *self);

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_3_32
GtkWidget     *ide_page_new                   (void);
IDE_AVAILABLE_IN_3_32
gboolean       ide_page_get_can_split         (IdePage              *self);
IDE_AVAILABLE_IN_3_32
void           ide_page_set_can_split         (IdePage              *self,
                                               gboolean              can_split);
IDE_AVAILABLE_IN_3_32
IdePage       *ide_page_create_split          (IdePage              *self);
IDE_AVAILABLE_IN_3_32
const gchar   *ide_page_get_icon_name         (IdePage              *self);
IDE_AVAILABLE_IN_3_32
void           ide_page_set_icon_name         (IdePage              *self,
                                               const gchar          *icon_name);
IDE_AVAILABLE_IN_3_32
GIcon         *ide_page_get_icon              (IdePage              *self);
IDE_AVAILABLE_IN_3_32
void           ide_page_set_icon              (IdePage              *self,
                                               GIcon                *icon);
IDE_AVAILABLE_IN_3_32
gboolean       ide_page_get_failed            (IdePage              *self);
IDE_AVAILABLE_IN_3_32
void           ide_page_set_failed            (IdePage              *self,
                                               gboolean              failed);
IDE_AVAILABLE_IN_3_32
const gchar   *ide_page_get_menu_id           (IdePage              *self);
IDE_AVAILABLE_IN_3_32
void           ide_page_set_menu_id           (IdePage              *self,
                                               const gchar          *menu_id);
IDE_AVAILABLE_IN_3_32
gboolean       ide_page_get_modified          (IdePage              *self);
IDE_AVAILABLE_IN_3_32
void           ide_page_set_modified          (IdePage              *self,
                                               gboolean              modified);
IDE_AVAILABLE_IN_3_32
const gchar   *ide_page_get_title             (IdePage              *self);
IDE_AVAILABLE_IN_3_32
void           ide_page_set_title             (IdePage              *self,
                                               const gchar          *title);
IDE_AVAILABLE_IN_3_32
const GdkRGBA *ide_page_get_primary_color_bg  (IdePage              *self);
IDE_AVAILABLE_IN_3_32
void           ide_page_set_primary_color_bg  (IdePage              *self,
                                               const GdkRGBA        *primary_color_bg);
IDE_AVAILABLE_IN_3_32
const GdkRGBA *ide_page_get_primary_color_fg  (IdePage              *self);
IDE_AVAILABLE_IN_3_32
void           ide_page_set_primary_color_fg  (IdePage              *self,
                                               const GdkRGBA        *primary_color_fg);
IDE_AVAILABLE_IN_3_32
void           ide_page_agree_to_close_async  (IdePage              *self,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean       ide_page_agree_to_close_finish (IdePage              *self,
                                               GAsyncResult         *result,
                                               GError              **error);
IDE_AVAILABLE_IN_3_32
void           ide_page_mark_used             (IdePage              *self);
IDE_AVAILABLE_IN_3_32
void           ide_page_report_error          (IdePage              *self,
                                               const gchar          *format,
                                               ...) G_GNUC_PRINTF (2, 3);

G_END_DECLS
