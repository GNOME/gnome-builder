/* ide-internal.h
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

#ifndef IDE_PRIVATE_H
#define IDE_PRIVATE_H

#include <gtksourceview/gtksource.h>

#include "ide-back-forward-item.h"
#include "ide-back-forward-list.h"
#include "ide-diagnostic.h"
#include "ide-types.h"
#include "ide-settings.h"
#include "ide-source-view.h"
#include "ide-source-view-mode.h"
#include "ide-symbol.h"
#include "ide-highlight-engine.h"

G_BEGIN_DECLS

void                _ide_battery_monitor_init               (void);
void                _ide_battery_monitor_shutdown           (void);
void                _ide_buffer_set_changed_on_volume       (IdeBuffer             *self,
                                                             gboolean               changed_on_volume);
gboolean            _ide_buffer_get_loading                 (IdeBuffer             *self);
void                _ide_buffer_set_loading                 (IdeBuffer             *self,
                                                             gboolean               loading);
void                _ide_buffer_set_mtime                   (IdeBuffer             *self,
                                                             const GTimeVal        *mtime);
void                _ide_buffer_set_read_only               (IdeBuffer             *buffer,
                                                             gboolean               read_only);
void                _ide_buffer_manager_reclaim             (IdeBufferManager      *self,
                                                             IdeBuffer             *buffer);
void                _ide_build_system_set_project_file      (IdeBuildSystem        *self,
                                                             GFile                 *project_file);
gboolean            _ide_context_is_restoring               (IdeContext            *self);
const gchar        *_ide_file_get_content_type              (IdeFile               *self);
GtkSourceFile      *_ide_file_set_content_type              (IdeFile               *self,
                                                             const gchar           *content_type);
GtkSourceFile      *_ide_file_get_source_file               (IdeFile               *self);
void                _ide_file_settings_add_child            (IdeFileSettings       *self,
                                                             IdeFileSettings       *child);
IdeFixit           *_ide_fixit_new                          (IdeSourceRange        *source_range,
                                                             const gchar           *replacement_text);
void                _ide_project_set_name                   (IdeProject            *project,
                                                             const gchar           *name);
void                _ide_runtime_manager_unload             (IdeRuntimeManager     *self);
void                _ide_search_context_add_provider        (IdeSearchContext      *context,
                                                             IdeSearchProvider     *provider,
                                                             gsize                  max_results);
void                _ide_service_emit_context_loaded        (IdeService            *service);
IdeSettings        *_ide_settings_new                       (IdeContext            *context,
                                                             const gchar           *schema_id,
                                                             const gchar           *relative_path,
                                                             gboolean               ignore_project_settings);
GtkTextMark        *_ide_source_view_get_scroll_mark        (IdeSourceView         *self);
gboolean            _ide_source_view_mode_do_event          (IdeSourceViewMode     *mode,
                                                             GdkEventKey           *event,
                                                             gboolean              *remove);
IdeSourceViewMode  *_ide_source_view_mode_new               (GtkWidget             *view,
                                                             const char            *mode,
                                                             IdeSourceViewModeType  type);
void                _ide_source_view_set_count              (IdeSourceView         *self,
                                                             gint                   count);
void                _ide_source_view_set_modifier           (IdeSourceView         *self,
                                                             gunichar               modifier);
void                _ide_thread_pool_init                   (gboolean               is_worker);
IdeUnsavedFile     *_ide_unsaved_file_new                   (GFile                 *file,
                                                             GBytes                *content,
                                                             const gchar           *temp_path,
                                                             gint64                 sequence);
void                _ide_highlighter_set_highlighter_engine (IdeHighlighter        *highlighter,
                                                             IdeHighlightEngine    *highlight_engine);
const gchar        *_ide_source_view_get_mode_name          (IdeSourceView         *self);

G_END_DECLS

#endif /* IDE_PRIVATE_H */
