/* ide-source-view-mode.h
 *
 * Copyright (C) 2015 Alexander Larsson <alexl@redhat.com>
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

#include "ide-types.h"
#include "sourceview/ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_VIEW_MODE (ide_source_view_mode_get_type())

G_DECLARE_FINAL_TYPE (IdeSourceViewMode, ide_source_view_mode, IDE, SOURCE_VIEW_MODE, GtkWidget)

gboolean               ide_source_view_mode_get_repeat_insert_with_count (IdeSourceViewMode     *self);
gboolean               ide_source_view_mode_get_block_cursor             (IdeSourceViewMode     *self);
gboolean               ide_source_view_mode_get_suppress_unbound         (IdeSourceViewMode     *self);
const gchar           *ide_source_view_mode_get_name                     (IdeSourceViewMode     *self);
const gchar           *ide_source_view_mode_get_default_mode             (IdeSourceViewMode     *self);
const gchar           *ide_source_view_mode_get_display_name             (IdeSourceViewMode     *self);
gboolean               ide_source_view_mode_get_keep_mark_on_char        (IdeSourceViewMode     *self);
IdeSourceViewModeType  ide_source_view_mode_get_mode_type                (IdeSourceViewMode     *self);
void                   ide_source_view_mode_set_has_indenter             (IdeSourceViewMode     *self,
                                                                          gboolean               has_indenter);
IdeSourceViewMode     *_ide_source_view_mode_new                         (GtkWidget             *view,
                                                                          const char            *mode,
                                                                          IdeSourceViewModeType  type) G_GNUC_INTERNAL;
gboolean               _ide_source_view_mode_do_event                    (IdeSourceViewMode     *mode,
                                                                          GdkEventKey           *event,
                                                                          gboolean              *remove) G_GNUC_INTERNAL;

G_END_DECLS
