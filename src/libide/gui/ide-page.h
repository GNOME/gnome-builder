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

#include <libpanel.h>

#include <libide-core.h>

#include "ide-panel-position.h"

G_BEGIN_DECLS

#define IDE_TYPE_PAGE (ide_page_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdePage, ide_page, IDE, PAGE, PanelWidget)

typedef void (*IdePageCallback) (IdePage  *page,
                                 gpointer  user_data);

struct _IdePageClass
{
  PanelWidgetClass parent_class;

  void           (*agree_to_close_async)  (IdePage              *self,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data);
  gboolean       (*agree_to_close_finish) (IdePage              *self,
                                           GAsyncResult         *result,
                                           GError              **error);
  IdePage       *(*create_split)          (IdePage              *self);
  GFile         *(*get_file_or_directory) (IdePage              *self);

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
gboolean       ide_page_get_can_split         (IdePage              *self);
IDE_AVAILABLE_IN_ALL
void           ide_page_set_can_split         (IdePage              *self,
                                               gboolean              can_split);
IDE_AVAILABLE_IN_ALL
IdePage       *ide_page_create_split          (IdePage              *self);
IDE_AVAILABLE_IN_ALL
gboolean       ide_page_get_failed            (IdePage              *self);
IDE_AVAILABLE_IN_ALL
void           ide_page_set_failed            (IdePage              *self,
                                               gboolean              failed);
IDE_AVAILABLE_IN_ALL
const gchar   *ide_page_get_menu_id           (IdePage              *self);
IDE_AVAILABLE_IN_ALL
void           ide_page_set_menu_id           (IdePage              *self,
                                               const gchar          *menu_id);
IDE_AVAILABLE_IN_ALL
void           ide_page_agree_to_close_async  (IdePage              *self,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean       ide_page_agree_to_close_finish (IdePage              *self,
                                               GAsyncResult         *result,
                                               GError              **error);
IDE_AVAILABLE_IN_ALL
void           ide_page_mark_used             (IdePage              *self);
IDE_AVAILABLE_IN_ALL
void           ide_page_report_error          (IdePage              *self,
                                               const gchar          *format,
                                               ...) G_GNUC_PRINTF (2, 3);
IDE_AVAILABLE_IN_ALL
GFile         *ide_page_get_file_or_directory (IdePage              *self);
IDE_AVAILABLE_IN_ALL
void           ide_page_set_progress          (IdePage              *self,
                                               IdeNotification      *notification);
IDE_AVAILABLE_IN_ALL
PanelPosition *ide_page_get_position          (IdePage              *self);
IDE_AVAILABLE_IN_ALL
void           ide_page_observe               (IdePage              *self,
                                               IdePage             **location);
IDE_AVAILABLE_IN_ALL
void           ide_page_unobserve             (IdePage              *self,
                                               IdePage             **location);
IDE_AVAILABLE_IN_ALL
void           ide_page_destroy               (IdePage              *self);
IDE_AVAILABLE_IN_ALL
void           ide_clear_page                 (IdePage             **location);
IDE_AVAILABLE_IN_ALL
void           ide_page_add_content_widget    (IdePage              *self,
                                               GtkWidget            *widget);

G_END_DECLS
