/* ide-hover-context.h
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

#if !defined (IDE_SOURCEVIEW_INSIDE) && !defined (IDE_SOURCEVIEW_COMPILATION)
# error "Only <libide-sourceview.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <libide-core.h>
#include <libide-code.h>

G_BEGIN_DECLS

#define IDE_TYPE_HOVER_CONTEXT (ide_hover_context_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeHoverContext, ide_hover_context, IDE, HOVER_CONTEXT, GObject)

IDE_AVAILABLE_IN_3_32
void     ide_hover_context_add_content (IdeHoverContext  *self,
                                        gint              priority,
                                        const gchar      *title,
                                        IdeMarkedContent *content);
IDE_AVAILABLE_IN_3_32
void     ide_hover_context_add_widget  (IdeHoverContext  *self,
                                        gint              priority,
                                        const gchar      *title,
                                        GtkWidget        *widget);
IDE_AVAILABLE_IN_3_32
gboolean ide_hover_context_has_content (IdeHoverContext  *self);

G_END_DECLS
