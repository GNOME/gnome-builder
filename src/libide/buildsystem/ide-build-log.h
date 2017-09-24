/* ide-build-log.h
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

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  IDE_BUILD_LOG_STDOUT,
  IDE_BUILD_LOG_STDERR,
} IdeBuildLogStream;

typedef void (*IdeBuildLogObserver) (IdeBuildLogStream  log_stream,
                                     const gchar       *message,
                                     gssize             message_len,
                                     gpointer           user_data);

G_END_DECLS
