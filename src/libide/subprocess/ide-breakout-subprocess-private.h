/* ide-breakout-subprocess-private.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "subprocess/ide-breakout-subprocess.h"

G_BEGIN_DECLS

typedef struct
{
  gint source_fd;
  gint dest_fd;
} IdeBreakoutFdMapping;

IdeSubprocess *_ide_breakout_subprocess_new (const gchar                 *cwd,
                                             const gchar * const         *argv,
                                             const gchar * const         *env,
                                             GSubprocessFlags             flags,
                                             gboolean                     clear_flags,
                                             gint                         stdin_fd,
                                             gint                         stdout_fd,
                                             gint                         stderr_fd,
                                             const IdeBreakoutFdMapping  *fd_map,
                                             guint                        fd_map_len,
                                             GCancellable                *cancellable,
                                             GError                     **error) G_GNUC_INTERNAL;

G_END_DECLS
