/* ide-context.h
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
 */

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "ide-types.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONTEXT (ide_context_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeContext, ide_context, IDE, CONTEXT, GObject)

IDE_AVAILABLE_IN_3_32
GFile                    *ide_context_get_project_file          (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeBufferManager         *ide_context_get_buffer_manager        (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeBuildManager          *ide_context_get_build_manager         (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeBuildSystem           *ide_context_get_build_system          (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeConfigurationManager  *ide_context_get_configuration_manager (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeDebugManager          *ide_context_get_debug_manager         (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeDiagnosticsManager    *ide_context_get_diagnostics_manager   (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeDeviceManager         *ide_context_get_device_manager        (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeProject               *ide_context_get_project               (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
GSettings                *ide_context_get_project_settings      (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
GtkRecentManager         *ide_context_get_recent_manager        (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeRunManager            *ide_context_get_run_manager           (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeRuntimeManager        *ide_context_get_runtime_manager       (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeToolchainManager      *ide_context_get_toolchain_manager     (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeSearchEngine          *ide_context_get_search_engine         (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeSettings              *ide_context_get_settings              (IdeContext           *self,
                                                                 const gchar          *schema_id,
                                                                 const gchar          *relative_path);
IDE_AVAILABLE_IN_3_32
IdeSnippetStorage        *ide_context_get_snippets              (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeTestManager           *ide_context_get_test_manager          (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeUnsavedFiles          *ide_context_get_unsaved_files         (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
IdeVcs                   *ide_context_get_vcs                   (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
gpointer                  ide_context_get_service_typed         (IdeContext           *self,
                                                                 GType                 service_type);
IDE_AVAILABLE_IN_3_32
void                      ide_context_unload_async              (IdeContext           *self,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean                  ide_context_unload_finish             (IdeContext           *self,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);
IDE_AVAILABLE_IN_3_32
void                      ide_context_new_async                 (GFile                *project_file,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
IDE_AVAILABLE_IN_3_32
IdeContext               *ide_context_new_finish                (GAsyncResult         *result,
                                                                 GError              **error);
IDE_AVAILABLE_IN_3_32
void                      ide_context_restore_async             (IdeContext           *self,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean                  ide_context_restore_finish            (IdeContext           *self,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);
IDE_AVAILABLE_IN_3_32
void                      ide_context_add_pausable              (IdeContext           *self,
                                                                 IdePausable          *pausable);
IDE_AVAILABLE_IN_3_32
void                      ide_context_remove_pausable           (IdeContext           *self,
                                                                 IdePausable          *pausable);
IDE_AVAILABLE_IN_3_32
void                      ide_context_hold                      (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
void                      ide_context_hold_for_object           (IdeContext           *self,
                                                                 gpointer              instance);
IDE_AVAILABLE_IN_3_32
void                      ide_context_release                   (IdeContext           *self);
IDE_AVAILABLE_IN_3_32
void                      ide_context_message                   (IdeContext           *self,
                                                                 const gchar          *format,
                                                                 ...) G_GNUC_PRINTF (2, 3);
IDE_AVAILABLE_IN_3_32
void                      ide_context_warning                   (IdeContext           *self,
                                                                 const gchar          *format,
                                                                 ...) G_GNUC_PRINTF (2, 3);
IDE_AVAILABLE_IN_3_32
void                      ide_context_emit_log                  (IdeContext           *self,
                                                                 GLogLevelFlags        log_level,
                                                                 const gchar          *message,
                                                                 gssize                message_len);
IDE_AVAILABLE_IN_3_32
gchar                    *ide_context_build_filename            (IdeContext           *self,
                                                                 const gchar          *first_part,
                                                                 ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_3_32
GFile                    *ide_context_cache_file                (IdeContext           *self,
                                                                 const gchar          *first_part,
                                                                 ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_3_32
gchar                    *ide_context_cache_filename            (IdeContext           *self,
                                                                 const gchar          *first_part,
                                                                 ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_3_32
IdeVcsMonitor            *ide_context_get_monitor               (IdeContext           *self);

IDE_AVAILABLE_IN_3_32
gboolean                  ide_context_is_unloading              (IdeContext           *self);

IDE_AVAILABLE_IN_3_32
GtkWidget                *ide_context_get_workbench             (IdeContext           *self);
GListModel               *_ide_context_get_pausables            (IdeContext           *self) G_GNUC_INTERNAL;
gboolean                  _ide_context_is_restoring             (IdeContext           *self) G_GNUC_INTERNAL;
void                      _ide_context_set_workbench            (IdeContext           *self,
                                                                 GtkWidget            *workbench) G_GNUC_INTERNAL;

G_END_DECLS
