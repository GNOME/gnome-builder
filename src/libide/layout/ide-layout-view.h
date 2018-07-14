/* ide-layout-view.h
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_LAYOUT_VIEW (ide_layout_view_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeLayoutView, ide_layout_view, IDE, LAYOUT_VIEW, GtkBox)

struct _IdeLayoutViewClass
{
  GtkBoxClass parent_class;

  void           (*agree_to_close_async)  (IdeLayoutView        *self,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data);
  gboolean       (*agree_to_close_finish) (IdeLayoutView        *self,
                                           GAsyncResult         *result,
                                           GError              **error);
  IdeLayoutView *(*create_split_view)     (IdeLayoutView        *self);

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
  gpointer _reserved9;
  gpointer _reserved10;
  gpointer _reserved11;
  gpointer _reserved12;
};

IDE_AVAILABLE_IN_ALL
GtkWidget     *ide_layout_view_new                   (void);
IDE_AVAILABLE_IN_ALL
gboolean       ide_layout_view_get_can_split         (IdeLayoutView        *self);
IDE_AVAILABLE_IN_ALL
void           ide_layout_view_set_can_split         (IdeLayoutView        *self,
                                                      gboolean              can_split);
IDE_AVAILABLE_IN_ALL
IdeLayoutView *ide_layout_view_create_split_view     (IdeLayoutView        *self);
IDE_AVAILABLE_IN_ALL
const gchar   *ide_layout_view_get_icon_name         (IdeLayoutView        *self);
IDE_AVAILABLE_IN_ALL
void           ide_layout_view_set_icon_name         (IdeLayoutView        *self,
                                                      const gchar          *icon_name);
IDE_AVAILABLE_IN_3_30
GIcon         *ide_layout_view_get_icon              (IdeLayoutView        *self);
IDE_AVAILABLE_IN_3_30
void           ide_layout_view_set_icon              (IdeLayoutView        *self,
                                                      GIcon                *icon);
IDE_AVAILABLE_IN_ALL
gboolean       ide_layout_view_get_failed            (IdeLayoutView        *self);
IDE_AVAILABLE_IN_ALL
void           ide_layout_view_set_failed            (IdeLayoutView        *self,
                                                      gboolean              failed);
IDE_AVAILABLE_IN_ALL
const gchar   *ide_layout_view_get_menu_id           (IdeLayoutView        *self);
IDE_AVAILABLE_IN_ALL
void           ide_layout_view_set_menu_id           (IdeLayoutView        *self,
                                                      const gchar          *menu_id);
IDE_AVAILABLE_IN_ALL
gboolean       ide_layout_view_get_modified          (IdeLayoutView        *self);
IDE_AVAILABLE_IN_ALL
void           ide_layout_view_set_modified          (IdeLayoutView        *self,
                                                      gboolean              modified);
IDE_AVAILABLE_IN_ALL
const gchar   *ide_layout_view_get_title             (IdeLayoutView        *self);
IDE_AVAILABLE_IN_ALL
void           ide_layout_view_set_title             (IdeLayoutView        *self,
                                                      const gchar          *title);
IDE_AVAILABLE_IN_ALL
const GdkRGBA *ide_layout_view_get_primary_color_bg  (IdeLayoutView        *self);
IDE_AVAILABLE_IN_ALL
void           ide_layout_view_set_primary_color_bg  (IdeLayoutView        *self,
                                                      const GdkRGBA        *primary_color_bg);
IDE_AVAILABLE_IN_ALL
const GdkRGBA *ide_layout_view_get_primary_color_fg  (IdeLayoutView        *self);
IDE_AVAILABLE_IN_ALL
void           ide_layout_view_set_primary_color_fg  (IdeLayoutView        *self,
                                                      const GdkRGBA        *primary_color_fg);
IDE_AVAILABLE_IN_ALL
void           ide_layout_view_agree_to_close_async  (IdeLayoutView        *self,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean       ide_layout_view_agree_to_close_finish (IdeLayoutView        *self,
                                                      GAsyncResult         *result,
                                                      GError              **error);
IDE_AVAILABLE_IN_ALL
void           ide_layout_view_report_error          (IdeLayoutView        *self,
                                                      const gchar          *format,
                                                      ...) G_GNUC_PRINTF (2, 3);

G_END_DECLS
