/* ide-formatter.h
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

#include "ide-object.h"
#include "ide-version-macros.h"

#include "formatting/ide-formatter-options.h"

G_BEGIN_DECLS

#define IDE_TYPE_FORMATTER (ide_formatter_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeFormatter, ide_formatter, IDE, FORMATTER, IdeObject)

struct _IdeFormatterInterface
{
  GTypeInterface parent;

  void     (*load)                (IdeFormatter         *self);
  void     (*format_async)        (IdeFormatter         *self,
                                   IdeBuffer            *buffer,
                                   IdeFormatterOptions  *options,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data);
  gboolean (*format_finish)       (IdeFormatter         *self,
                                   GAsyncResult         *result,
                                   GError              **error);
  void     (*format_range_async)  (IdeFormatter         *self,
                                   IdeBuffer            *buffer,
                                   IdeFormatterOptions  *options,
                                   const GtkTextIter    *begin,
                                   const GtkTextIter    *end,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data);
  gboolean (*format_range_finish) (IdeFormatter         *self,
                                   GAsyncResult         *result,
                                   GError              **error);
};

IDE_AVAILABLE_IN_ALL
void     ide_formatter_load                (IdeFormatter         *self);
IDE_AVAILABLE_IN_ALL
void     ide_formatter_format_async        (IdeFormatter         *self,
                                            IdeBuffer            *buffer,
                                            IdeFormatterOptions  *options,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_formatter_format_finish       (IdeFormatter         *self,
                                            GAsyncResult         *result,
                                            GError              **error);
IDE_AVAILABLE_IN_ALL
void     ide_formatter_format_range_async  (IdeFormatter         *self,
                                            IdeBuffer            *buffer,
                                            IdeFormatterOptions  *options,
                                            const GtkTextIter    *begin,
                                            const GtkTextIter    *end,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_formatter_format_range_finish (IdeFormatter         *self,
                                            GAsyncResult         *result,
                                            GError              **error);

G_END_DECLS
