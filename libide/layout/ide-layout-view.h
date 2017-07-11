/* ide-layout-view.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

G_BEGIN_DECLS

#define IDE_TYPE_LAYOUT_VIEW (ide_layout_view_get_type())

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

GtkWidget     *ide_layout_view_new                   (void);
gboolean       ide_layout_view_get_can_split         (IdeLayoutView        *self);
void           ide_layout_view_set_can_split         (IdeLayoutView        *self,
                                                      gboolean              can_split);
IdeLayoutView *ide_layout_view_create_split_view     (IdeLayoutView        *self);
const gchar   *ide_layout_view_get_icon_name         (IdeLayoutView        *self);
void           ide_layout_view_set_icon_name         (IdeLayoutView        *self,
                                                      const gchar          *icon_name);
gboolean       ide_layout_view_get_failed            (IdeLayoutView        *self);
void           ide_layout_view_set_failed            (IdeLayoutView        *self,
                                                      gboolean              failed);
const gchar   *ide_layout_view_get_menu_id           (IdeLayoutView        *self);
void           ide_layout_view_set_menu_id           (IdeLayoutView        *self,
                                                      const gchar          *menu_id);
gboolean       ide_layout_view_get_modified          (IdeLayoutView        *self);
void           ide_layout_view_set_modified          (IdeLayoutView        *self,
                                                      gboolean              modified);
const gchar   *ide_layout_view_get_title             (IdeLayoutView        *self);
void           ide_layout_view_set_title             (IdeLayoutView        *self,
                                                      const gchar          *title);
const GdkRGBA *ide_layout_view_get_primary_color_bg  (IdeLayoutView        *self);
void           ide_layout_view_set_primary_color_bg  (IdeLayoutView        *self,
                                                      const GdkRGBA        *primary_color_bg);
const GdkRGBA *ide_layout_view_get_primary_color_fg  (IdeLayoutView        *self);
void           ide_layout_view_set_primary_color_fg  (IdeLayoutView        *self,
                                                      const GdkRGBA        *primary_color_fg);
void           ide_layout_view_agree_to_close_async  (IdeLayoutView        *self,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
gboolean       ide_layout_view_agree_to_close_finish (IdeLayoutView        *self,
                                                      GAsyncResult         *result,
                                                      GError              **error);
void           ide_layout_view_report_error          (IdeLayoutView        *self,
                                                      const gchar          *format,
                                                      ...) G_GNUC_PRINTF (2, 3);

G_END_DECLS
