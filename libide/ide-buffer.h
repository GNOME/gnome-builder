/* ide-buffer.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_BUFFER_H
#define IDE_BUFFER_H

#include <gtksourceview/gtksource.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUFFER (ide_buffer_get_type ())

#define IDE_BUFFER_LINE_FLAGS_DIAGNOSTICS_MASK \
  ((IDE_BUFFER_LINE_FLAGS_ERROR | IDE_BUFFER_LINE_FLAGS_WARNING | IDE_BUFFER_LINE_FLAGS_NOTE))

G_DECLARE_DERIVABLE_TYPE (IdeBuffer, ide_buffer, IDE, BUFFER, GtkSourceBuffer)

typedef enum
{
  IDE_BUFFER_LINE_FLAGS_NONE     = 0,
  IDE_BUFFER_LINE_FLAGS_ADDED    = 1 << 0,
  IDE_BUFFER_LINE_FLAGS_CHANGED  = 1 << 1,
  IDE_BUFFER_LINE_FLAGS_ERROR    = 1 << 2,
  IDE_BUFFER_LINE_FLAGS_WARNING  = 1 << 3,
  IDE_BUFFER_LINE_FLAGS_NOTE     = 1 << 4,
} IdeBufferLineFlags;

struct _IdeBufferClass
{
  GtkSourceBufferClass parent_class;

  void (*cursor_moved) (IdeBuffer         *self,
                        const GtkTextIter *location);
};

gboolean            ide_buffer_get_changed_on_volume         (IdeBuffer            *self);
GBytes             *ide_buffer_get_content                   (IdeBuffer            *self);
IdeContext         *ide_buffer_get_context                   (IdeBuffer            *self);
IdeDiagnostic      *ide_buffer_get_diagnostic_at_iter        (IdeBuffer            *self,
                                                              const GtkTextIter    *iter);
IdeFile            *ide_buffer_get_file                      (IdeBuffer            *self);
IdeBufferLineFlags  ide_buffer_get_line_flags                (IdeBuffer            *buffer,
                                                              guint                 line);
gboolean            ide_buffer_get_read_only                 (IdeBuffer            *self);
gboolean            ide_buffer_get_highlight_diagnostics     (IdeBuffer            *self);
const gchar        *ide_buffer_get_style_scheme_name         (IdeBuffer            *self);
const gchar        *ide_buffer_get_title                     (IdeBuffer            *self);
void                ide_buffer_set_file                      (IdeBuffer            *self,
                                                              IdeFile              *file);
void                ide_buffer_set_highlight_diagnostics     (IdeBuffer            *self,
                                                              gboolean              highlight_diagnostics);
void                ide_buffer_set_style_scheme_name         (IdeBuffer            *self,
                                                              const gchar          *style_scheme_name);
void                ide_buffer_trim_trailing_whitespace      (IdeBuffer            *self);
void                ide_buffer_check_for_volume_change       (IdeBuffer            *self);
void                ide_buffer_get_iter_at_source_location   (IdeBuffer            *self,
                                                              GtkTextIter          *iter,
                                                              IdeSourceLocation    *location);
void                ide_buffer_rehighlight                   (IdeBuffer            *self);
void                ide_buffer_get_selection_bounds          (IdeBuffer            *self,
                                                              GtkTextIter          *insert,
                                                              GtkTextIter          *selection);
IdeSymbolResolver  *ide_buffer_get_symbol_resolver           (IdeBuffer            *self);
void                ide_buffer_get_symbol_at_location_async  (IdeBuffer            *self,
                                                              const GtkTextIter    *location,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
IdeSymbol          *ide_buffer_get_symbol_at_location_finish (IdeBuffer            *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
void                ide_buffer_get_symbols_async             (IdeBuffer            *self,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
GPtrArray          *ide_buffer_get_symbols_finish            (IdeBuffer            *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
void                ide_buffer_hold                          (IdeBuffer            *self);
void                ide_buffer_release                       (IdeBuffer            *self);

G_END_DECLS

#endif /* IDE_BUFFER_H */
