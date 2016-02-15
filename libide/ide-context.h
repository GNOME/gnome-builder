/* ide-context.h
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

#ifndef IDE_CONTEXT_H
#define IDE_CONTEXT_H

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONTEXT (ide_context_get_type())

G_DECLARE_FINAL_TYPE (IdeContext, ide_context, IDE, CONTEXT, GObject)

IdeBackForwardList       *ide_context_get_back_forward_list     (IdeContext           *self);
GFile                    *ide_context_get_project_file          (IdeContext           *self);
IdeBufferManager         *ide_context_get_buffer_manager        (IdeContext           *self);
IdeBuildSystem           *ide_context_get_build_system          (IdeContext           *self);
IdeConfigurationManager  *ide_context_get_configuration_manager (IdeContext           *self);
IdeDeviceManager         *ide_context_get_device_manager        (IdeContext           *self);
IdeProject               *ide_context_get_project               (IdeContext           *self);
GtkRecentManager         *ide_context_get_recent_manager        (IdeContext           *self);
IdeRuntimeManager        *ide_context_get_runtime_manager       (IdeContext           *self);
IdeScriptManager         *ide_context_get_script_manager        (IdeContext           *self);
IdeSearchEngine          *ide_context_get_search_engine         (IdeContext           *self);
IdeSettings              *ide_context_get_settings              (IdeContext           *self,
                                                                 const gchar          *schema_id,
                                                                 const gchar          *relative_path);
IdeSourceSnippetsManager *ide_context_get_snippets_manager      (IdeContext           *self);
IdeUnsavedFiles          *ide_context_get_unsaved_files         (IdeContext           *self);
IdeVcs                   *ide_context_get_vcs                   (IdeContext           *self);
const gchar              *ide_context_get_root_build_dir        (IdeContext           *self);
gpointer                  ide_context_get_service_typed         (IdeContext           *self,
                                                                 GType                 service_type);
void                      ide_context_unload_async              (IdeContext           *self,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
gboolean                  ide_context_unload_finish             (IdeContext           *self,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);
void                      ide_context_new_async                 (GFile                *project_file,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
IdeContext               *ide_context_new_finish                (GAsyncResult         *result,
                                                                 GError              **error);
void                      ide_context_set_root_build_dir        (IdeContext           *self,
                                                                 const gchar          *root_build_dir);
void                      ide_context_restore_async             (IdeContext           *self,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
gboolean                  ide_context_restore_finish            (IdeContext           *self,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);
void                      ide_context_hold                      (IdeContext           *self);
void                      ide_context_hold_for_object           (IdeContext           *self,
                                                                 gpointer              instance);
void                      ide_context_release                   (IdeContext           *self);
void                      ide_context_warning                   (IdeContext           *self,
                                                                 const gchar          *format,
                                                                 ...) G_GNUC_PRINTF (2, 3);

G_END_DECLS

#endif /* IDE_CONTEXT_H */
