/* ide-persistent-map-builder.h
 *
 * Copyright © 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#include <glib-object.h>
#include <gio/gio.h>

#pragma once

G_BEGIN_DECLS

#define IDE_TYPE_PERSISTENT_MAP_BUILDER (ide_persistent_map_builder_get_type ())

G_DECLARE_FINAL_TYPE (IdePersistentMapBuilder, ide_persistent_map_builder, IDE, PERSISTENT_MAP_BUILDER, GObject)

IdePersistentMapBuilder  *ide_persistent_map_builder_new                  (void);
void                      ide_persistent_map_builder_insert               (IdePersistentMapBuilder  *self,
                                                                           const gchar              *key,
                                                                           GVariant                 *value,
                                                                           gboolean                  replace);
gboolean                  ide_persistent_map_builder_write                (IdePersistentMapBuilder  *self,
                                                                           GFile                    *destination,
                                                                           gint                      io_priority,
                                                                           GCancellable             *cancellable,
                                                                           GError                  **error);
void                      ide_persistent_map_builder_write_async          (IdePersistentMapBuilder  *self,
                                                                           GFile                    *destination,
                                                                           gint                      io_priority,
                                                                           GCancellable             *cancellable,
                                                                           GAsyncReadyCallback       callback,
                                                                           gpointer                  user_data);
gboolean                  ide_persistent_map_builder_write_finish         (IdePersistentMapBuilder  *self,
                                                                           GAsyncResult             *result,
                                                                           GError                  **error);
void                      ide_persistent_map_builder_set_metadata_int64   (IdePersistentMapBuilder  *self,
                                                                           const gchar              *key,
                                                                           gint64                    value);

G_END_DECLS
