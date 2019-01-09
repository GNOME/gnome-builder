/* ide-buffer.h
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <gtksourceview/gtksource.h>
#include <libide-core.h>

#include "ide-buffer-change-monitor.h"
#include "ide-diagnostics.h"
#include "ide-file-settings.h"
#include "ide-formatter.h"
#include "ide-location.h"
#include "ide-range.h"
#include "ide-rename-provider.h"
#include "ide-symbol.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUFFER (ide_buffer_get_type())

typedef enum
{
  IDE_BUFFER_STATE_READY,
  IDE_BUFFER_STATE_LOADING,
  IDE_BUFFER_STATE_SAVING,
  IDE_BUFFER_STATE_FAILED,
} IdeBufferState;

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeBuffer, ide_buffer, IDE, BUFFER, GtkSourceBuffer)

IDE_AVAILABLE_IN_3_32
GBytes                 *ide_buffer_dup_content                   (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
gchar                  *ide_buffer_dup_title                     (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_format_selection_async        (IdeBuffer               *self,
                                                                  IdeFormatterOptions     *options,
                                                                  GCancellable            *cancellable,
                                                                  GAsyncReadyCallback      callback,
                                                                  gpointer                 user_data);
IDE_AVAILABLE_IN_3_32
gboolean                ide_buffer_format_selection_finish       (IdeBuffer               *self,
                                                                  GAsyncResult            *result,
                                                                  GError                 **error);
IDE_AVAILABLE_IN_3_32
guint                   ide_buffer_get_change_count              (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
IdeBufferChangeMonitor *ide_buffer_get_change_monitor            (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
gboolean                ide_buffer_get_changed_on_volume         (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
IdeDiagnostics         *ide_buffer_get_diagnostics               (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
IdeLocation            *ide_buffer_get_insert_location           (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
gboolean                ide_buffer_get_is_temporary              (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
gboolean                ide_buffer_get_failed                    (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
const GError           *ide_buffer_get_failure                   (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
gchar                  *ide_buffer_dup_uri                       (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
GFile                  *ide_buffer_get_file                      (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
IdeFileSettings        *ide_buffer_get_file_settings             (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
IdeFormatter           *ide_buffer_get_formatter                 (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
gboolean                ide_buffer_get_highlight_diagnostics     (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_get_iter_at_location          (IdeBuffer               *self,
                                                                  GtkTextIter             *iter,
                                                                  IdeLocation             *location);
IDE_AVAILABLE_IN_3_32
IdeLocation            *ide_buffer_get_iter_location             (IdeBuffer               *self,
                                                                  const GtkTextIter       *iter);
IDE_AVAILABLE_IN_3_32
const gchar            *ide_buffer_get_language_id               (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_set_language_id               (IdeBuffer               *self,
                                                                  const gchar             *language_id);
IDE_AVAILABLE_IN_3_32
gchar                  *ide_buffer_get_line_text                 (IdeBuffer               *self,
                                                                  guint                    line);
IDE_AVAILABLE_IN_3_32
gboolean                ide_buffer_get_loading                   (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
gboolean                ide_buffer_get_read_only                 (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
IdeRenameProvider      *ide_buffer_get_rename_provider           (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_get_selection_bounds          (IdeBuffer               *self,
                                                                  GtkTextIter             *insert,
                                                                  GtkTextIter             *selection);
IDE_AVAILABLE_IN_3_32
IdeRange               *ide_buffer_get_selection_range           (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
IdeBufferState          ide_buffer_get_state                     (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
const gchar            *ide_buffer_get_style_scheme_name         (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_get_symbol_at_location_async  (IdeBuffer               *self,
                                                                  const GtkTextIter       *location,
                                                                  GCancellable            *cancellable,
                                                                  GAsyncReadyCallback      callback,
                                                                  gpointer                 user_data);
IDE_AVAILABLE_IN_3_32
IdeSymbol              *ide_buffer_get_symbol_at_location_finish (IdeBuffer               *self,
                                                                  GAsyncResult            *result,
                                                                  GError                 **error);
IDE_AVAILABLE_IN_3_32
GPtrArray              *ide_buffer_get_symbol_resolvers          (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
gchar                  *ide_buffer_get_word_at_iter              (IdeBuffer               *self,
                                                                  const GtkTextIter       *iter);
IDE_AVAILABLE_IN_3_32
gboolean                ide_buffer_has_diagnostics               (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
gboolean                ide_buffer_has_symbol_resolvers          (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
IdeBuffer              *ide_buffer_hold                          (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
IdeContext             *ide_buffer_ref_context                   (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_rehighlight                   (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_release                       (IdeBuffer               *self);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_save_file_async               (IdeBuffer               *self,
                                                                  GFile                   *file,
                                                                  GCancellable            *cancellable,
                                                                  IdeNotification        **notif,
                                                                  GAsyncReadyCallback      callback,
                                                                  gpointer                 user_data);
IDE_AVAILABLE_IN_3_32
gboolean                ide_buffer_save_file_finish              (IdeBuffer               *self,
                                                                  GAsyncResult            *result,
                                                                  GError                 **error);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_set_change_monitor            (IdeBuffer               *self,
                                                                  IdeBufferChangeMonitor  *change_monitor);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_set_diagnostics               (IdeBuffer               *self,
                                                                  IdeDiagnostics          *diagnostics);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_set_highlight_diagnostics     (IdeBuffer               *self,
                                                                  gboolean                 highlight_diagnostics);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_set_style_scheme_name         (IdeBuffer               *self,
                                                                  const gchar             *style_scheme_name);
IDE_AVAILABLE_IN_3_32
void                    ide_buffer_trim_trailing_whitespace      (IdeBuffer               *self);

G_END_DECLS
