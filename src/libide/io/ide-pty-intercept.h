/* ide-pty-intercept.h
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

#if !defined (IDE_IO_INSIDE) && !defined (IDE_IO_COMPILATION)
# error "Only <libide-io.h> can be included directly."
#endif

#include <libide-core.h>
#include <unistd.h>

G_BEGIN_DECLS

#define IDE_PTY_FD_INVALID (-1)
#define IDE_PTY_INTERCEPT_MAGIC (0x81723647)
#define IDE_IS_PTY_INTERCEPT(s) ((s) != NULL && (s)->magic == IDE_PTY_INTERCEPT_MAGIC)

typedef int                              IdePtyFd;
typedef struct _IdePtyIntercept          IdePtyIntercept;
typedef struct _IdePtyInterceptSide      IdePtyInterceptSide;
typedef void (*IdePtyInterceptCallback) (const IdePtyIntercept     *intercept,
                                         const IdePtyInterceptSide *side,
                                         const guint8              *data,
                                         gsize                      len,
                                         gpointer                   user_data);

struct _IdePtyInterceptSide
{
  GIOChannel              *channel;
  guint                    in_watch;
  guint                    out_watch;
  gint                     read_prio;
  gint                     write_prio;
  GBytes                  *out_bytes;
  IdePtyInterceptCallback  callback;
  gpointer                 callback_data;
};

struct _IdePtyIntercept
{
  gsize               magic;
  IdePtyInterceptSide consumer;
  IdePtyInterceptSide producer;
};

static inline IdePtyFd
pty_fd_steal (IdePtyFd *fd)
{
  IdePtyFd ret = *fd;
  *fd = -1;
  return ret;
}

static void
pty_fd_clear (IdePtyFd *fd)
{
  if (fd != NULL && *fd != -1)
    {
      int rfd = *fd;
      *fd = -1;
      close (rfd);
    }
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (IdePtyFd, pty_fd_clear)

IDE_AVAILABLE_IN_ALL
IdePtyFd ide_pty_intercept_create_consumer (void);
IDE_AVAILABLE_IN_ALL
IdePtyFd ide_pty_intercept_create_producer (IdePtyFd                 consumer_fd,
                                            gboolean                 blocking);
IDE_AVAILABLE_IN_ALL
gboolean ide_pty_intercept_init          (IdePtyIntercept         *self,
                                          IdePtyFd                 fd,
                                          GMainContext            *main_context);
IDE_AVAILABLE_IN_ALL
IdePtyFd ide_pty_intercept_get_fd        (IdePtyIntercept         *self);
IDE_AVAILABLE_IN_ALL
gboolean ide_pty_intercept_set_size      (IdePtyIntercept         *self,
                                          guint                    rows,
                                          guint                    columns);
IDE_AVAILABLE_IN_ALL
void     ide_pty_intercept_clear         (IdePtyIntercept         *self);
IDE_AVAILABLE_IN_ALL
void     ide_pty_intercept_set_callback  (IdePtyIntercept         *self,
                                          IdePtyInterceptSide     *side,
                                          IdePtyInterceptCallback  callback,
                                          gpointer                 user_data);

G_END_DECLS
