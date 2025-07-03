/*
 * ide-scrollbar.h
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <libide-sourceview.h>

G_BEGIN_DECLS

#define IDE_TYPE_SCROLLBAR (ide_scrollbar_get_type())

#define IDE_LINE_CHANGES_FALLBACK_ADDED   "#26a269"
#define IDE_LINE_CHANGES_FALLBACK_CHANGED "#e5a50a"
#define IDE_LINE_CHANGES_FALLBACK_REMOVED "#c01c28"
#define IDE_DIAGNOSTIC_FALLBACK_ERROR     "#ff4444"
#define IDE_DIAGNOSTIC_FALLBACK_FATAL     "#cc0000"
#define IDE_DIAGNOSTIC_FALLBACK_WARNING   "#ffaa00"
#define IDE_DIAGNOSTIC_FALLBACK_DEPRECATED "#8888ff"

G_DECLARE_FINAL_TYPE(IdeScrollbar, ide_scrollbar, IDE, SCROLLBAR, GtkWidget)

void ide_scrollbar_set_view (IdeScrollbar *self, IdeSourceView *view);

G_END_DECLS

