/* ide-hover-context-private.h
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

#include <gtk/gtk.h>
#include <libide-code.h>

#include "ide-hover-context.h"
#include "ide-hover-provider.h"

G_BEGIN_DECLS

typedef void (*IdeHoverContextForeach) (const gchar      *title,
                                        IdeMarkedContent *content,
                                        GtkWidget        *widget,
                                        gpointer          user_data);

void     _ide_hover_context_add_provider  (IdeHoverContext         *context,
                                           IdeHoverProvider        *provider);
void     _ide_hover_context_query_async   (IdeHoverContext         *self,
                                           const GtkTextIter       *iter,
                                           GCancellable            *cancellable,
                                           GAsyncReadyCallback      callback,
                                           gpointer                 user_data);
gboolean _ide_hover_context_query_finish  (IdeHoverContext         *self,
                                           GAsyncResult            *result,
                                           GError                 **error);
void     _ide_hover_context_foreach       (IdeHoverContext         *self,
                                           IdeHoverContextForeach   foreach,
                                           gpointer                 foreach_data);

G_END_DECLS
