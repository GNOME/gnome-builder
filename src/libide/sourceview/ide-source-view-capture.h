/* ide-source-view-capture.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include "ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_VIEW_CAPTURE (ide_source_view_capture_get_type())

G_DECLARE_FINAL_TYPE (IdeSourceViewCapture, ide_source_view_capture, IDE, SOURCE_VIEW_CAPTURE, GObject)

IdeSourceViewCapture *ide_source_view_capture_new             (IdeSourceView         *view,
                                                               const gchar           *mode_name,
                                                               IdeSourceViewModeType  type,
                                                               guint                  count,
                                                               gunichar               modifier);
IdeSourceView        *ide_source_view_capture_get_view        (IdeSourceViewCapture  *self);
void                  ide_source_view_capture_replay          (IdeSourceViewCapture  *self);
void                  ide_source_view_capture_record_event    (IdeSourceViewCapture  *self,
                                                               const GdkEvent        *event,
                                                               guint                  count,
                                                               gunichar               modifier);
void                  ide_source_view_capture_record_modifier (IdeSourceViewCapture  *self,
                                                               gunichar               modifier);

G_END_DECLS
