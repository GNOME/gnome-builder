/* ide-buffer.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include <gtksourceview/gtksource.h>

#include "ide-version-macros.h"

#include "plugins/ide-extension-set-adapter.h"
#include "ide-types.h"

#include "formatting/ide-formatter-options.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUFFER (ide_buffer_get_type ())

#define IDE_BUFFER_LINE_FLAGS_DIAGNOSTICS_MASK \
  ((IDE_BUFFER_LINE_FLAGS_ERROR | IDE_BUFFER_LINE_FLAGS_WARNING | IDE_BUFFER_LINE_FLAGS_NOTE))

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeBuffer, ide_buffer, IDE, BUFFER, GtkSourceBuffer)

typedef enum
{
  IDE_BUFFER_LINE_FLAGS_NONE     = 0,
  IDE_BUFFER_LINE_FLAGS_ADDED    = 1 << 0,
  IDE_BUFFER_LINE_FLAGS_CHANGED  = 1 << 1,
  IDE_BUFFER_LINE_FLAGS_DELETED  = 1 << 2,
  IDE_BUFFER_LINE_FLAGS_ERROR    = 1 << 3,
  IDE_BUFFER_LINE_FLAGS_WARNING  = 1 << 4,
  IDE_BUFFER_LINE_FLAGS_NOTE     = 1 << 5,
} IdeBufferLineFlags;

struct _IdeBufferClass
{
  GtkSourceBufferClass parent_class;

  void (*cursor_moved) (IdeBuffer         *self,
                        const GtkTextIter *location);

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
void                      ide_buffer_format_selection_async        (IdeBuffer            *self,
                                                                    IdeFormatterOptions *options,
                                                                    GCancellable         *cancellable,
                                                                    GAsyncReadyCallback   callback,
                                                                    gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_format_selection_finish       (IdeBuffer            *self,
                                                                    GAsyncResult         *result,
                                                                    GError              **error);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_get_busy                      (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_get_changed_on_volume         (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
gsize                     ide_buffer_get_change_count              (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
GBytes                   *ide_buffer_get_content                   (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
IdeContext               *ide_buffer_get_context                   (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
IdeDiagnostic            *ide_buffer_get_diagnostic_at_iter        (IdeBuffer            *self,
                                                                    const GtkTextIter    *iter);
IDE_AVAILABLE_IN_ALL
IdeFile                  *ide_buffer_get_file                      (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
IdeBufferLineFlags        ide_buffer_get_line_flags                (IdeBuffer            *self,
                                                                    guint                 line);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_get_read_only                 (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_get_spell_checking            (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_get_highlight_diagnostics     (IdeBuffer            *self);
IDE_AVAILABLE_IN_3_30
gboolean                  ide_buffer_get_loading                   (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
const gchar              *ide_buffer_get_style_scheme_name         (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
const gchar              *ide_buffer_get_title                     (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
gchar                    *ide_buffer_get_uri                       (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_set_file                      (IdeBuffer            *self,
                                                                    IdeFile              *file);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_set_highlight_diagnostics     (IdeBuffer            *self,
                                                                    gboolean              highlight_diagnostics);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_set_spell_checking            (IdeBuffer            *self,
                                                                    gboolean              enable);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_set_style_scheme_name         (IdeBuffer            *self,
                                                                    const gchar          *style_scheme_name);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_trim_trailing_whitespace      (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_check_for_volume_change       (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
IdeSourceLocation        *ide_buffer_get_insert_location           (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
IdeSourceLocation        *ide_buffer_get_iter_location             (IdeBuffer            *self,
                                                                    const GtkTextIter    *iter);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_get_iter_at_source_location   (IdeBuffer            *self,
                                                                    GtkTextIter          *iter,
                                                                    IdeSourceLocation    *location);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_rehighlight                   (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_get_selection_bounds          (IdeBuffer            *self,
                                                                    GtkTextIter          *insert,
                                                                    GtkTextIter          *selection);
IDE_AVAILABLE_IN_ALL
IdeRenameProvider        *ide_buffer_get_rename_provider           (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
IdeExtensionSetAdapter   *ide_buffer_get_symbol_resolvers          (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_get_symbol_at_location_async  (IdeBuffer            *self,
                                                                    const GtkTextIter    *location,
                                                                    GCancellable         *cancellable,
                                                                    GAsyncReadyCallback   callback,
                                                                    gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeSymbol                *ide_buffer_get_symbol_at_location_finish (IdeBuffer            *self,
                                                                    GAsyncResult         *result,
                                                                    GError              **error);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_hold                          (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_release                       (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
gchar                    *ide_buffer_get_word_at_iter              (IdeBuffer            *self,
                                                                    const GtkTextIter    *iter);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_sync_to_unsaved_files         (IdeBuffer            *self);
IDE_AVAILABLE_IN_3_28
gboolean                  ide_buffer_get_failed                    (IdeBuffer            *self);
IDE_AVAILABLE_IN_3_28
const GError             *ide_buffer_get_failure                   (IdeBuffer            *self);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_get_has_diagnostics           (IdeBuffer            *self);
IDE_AVAILABLE_IN_3_30
void                      ide_buffer_get_build_flags_async         (IdeBuffer            *self,
                                                                    GCancellable         *cancellable,
                                                                    GAsyncReadyCallback   callback,
                                                                    gpointer              user_data);
IDE_AVAILABLE_IN_3_30
gchar                   **ide_buffer_get_build_flags_finish        (IdeBuffer            *self,
                                                                    GAsyncResult         *result,
                                                                    GError              **error);
IDE_AVAILABLE_IN_3_30
const gchar              *ide_buffer_get_language_id               (IdeBuffer            *self);

G_END_DECLS
