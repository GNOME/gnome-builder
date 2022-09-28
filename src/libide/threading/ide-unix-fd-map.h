/* ide-unix-fd-map.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_THREADING_INSIDE) && !defined (IDE_THREADING_COMPILATION)
# error "Only <libide-threading.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_UNIX_FD_MAP (ide_unix_fd_map_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeUnixFDMap, ide_unix_fd_map, IDE, UNIX_FD_MAP, GObject)

IDE_AVAILABLE_IN_ALL
IdeUnixFDMap *ide_unix_fd_map_new             (void);
IDE_AVAILABLE_IN_ALL
guint         ide_unix_fd_map_get_length      (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
int           ide_unix_fd_map_peek_stdin      (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
int           ide_unix_fd_map_peek_stdout     (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
int           ide_unix_fd_map_peek_stderr     (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
int           ide_unix_fd_map_steal_stdin     (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
int           ide_unix_fd_map_steal_stdout    (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
int           ide_unix_fd_map_steal_stderr    (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
gboolean      ide_unix_fd_map_steal_from      (IdeUnixFDMap  *self,
                                               IdeUnixFDMap  *other,
                                               GError       **error);
IDE_AVAILABLE_IN_ALL
int           ide_unix_fd_map_peek            (IdeUnixFDMap  *self,
                                               guint          index,
                                               int           *dest_fd);
IDE_AVAILABLE_IN_ALL
int           ide_unix_fd_map_get             (IdeUnixFDMap  *self,
                                               guint          index,
                                               int           *dest_fd,
                                               GError       **error);
IDE_AVAILABLE_IN_ALL
int           ide_unix_fd_map_steal           (IdeUnixFDMap  *self,
                                               guint          index,
                                               int           *dest_fd);
IDE_AVAILABLE_IN_ALL
void          ide_unix_fd_map_take            (IdeUnixFDMap  *self,
                                               int            source_fd,
                                               int            dest_fd);
IDE_AVAILABLE_IN_ALL
gboolean      ide_unix_fd_map_open_file       (IdeUnixFDMap  *self,
                                               const char    *filename,
                                               int            mode,
                                               int            dest_fd,
                                               GError       **error);
IDE_AVAILABLE_IN_ALL
int           ide_unix_fd_map_get_max_dest_fd (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
gboolean      ide_unix_fd_map_stdin_isatty    (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
gboolean      ide_unix_fd_map_stdout_isatty   (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
gboolean      ide_unix_fd_map_stderr_isatty   (IdeUnixFDMap  *self);
IDE_AVAILABLE_IN_ALL
GIOStream    *ide_unix_fd_map_create_stream   (IdeUnixFDMap  *self,
                                               int            dest_read_fd,
                                               int            dest_write_fd,
                                               GError       **error);
IDE_AVAILABLE_IN_44
gboolean      ide_unix_fd_map_silence_fd      (IdeUnixFDMap  *self,
                                               int            dest_fd,
                                               GError       **error);

G_END_DECLS
