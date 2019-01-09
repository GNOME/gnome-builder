/* ide-hover-provider.h
 *
 * Copyright 2018-2019 Christian Hergert <christian@hergert.me>
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

#include "ide-hover-context.h"
#include "ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_HOVER_PROVIDER (ide_hover_provider_get_type ())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeHoverProvider, ide_hover_provider, IDE, HOVER_PROVIDER, GObject)

struct _IdeHoverProviderInterface
{
  GTypeInterface parent;

  void     (*load)         (IdeHoverProvider     *self,
                            IdeSourceView        *view);
  void     (*unload)       (IdeHoverProvider     *self,
                            IdeSourceView        *view);
  void     (*hover_async)  (IdeHoverProvider     *self,
                            IdeHoverContext      *context,
                            const GtkTextIter    *location,
                            GCancellable         *cancellable,
                            GAsyncReadyCallback   callback,
                            gpointer              user_data);
  gboolean (*hover_finish) (IdeHoverProvider     *self,
                            GAsyncResult         *result,
                            GError              **error);
};

IDE_AVAILABLE_IN_3_32
void     ide_hover_provider_load         (IdeHoverProvider     *self,
                                          IdeSourceView        *view);
IDE_AVAILABLE_IN_3_32
void     ide_hover_provider_unload       (IdeHoverProvider     *self,
                                          IdeSourceView        *view);
IDE_AVAILABLE_IN_3_32
void     ide_hover_provider_hover_async  (IdeHoverProvider     *self,
                                          IdeHoverContext      *context,
                                          const GtkTextIter    *location,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean ide_hover_provider_hover_finish (IdeHoverProvider     *self,
                                          GAsyncResult         *result,
                                          GError              **error);

G_END_DECLS
