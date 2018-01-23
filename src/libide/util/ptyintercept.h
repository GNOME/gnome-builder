/* ptyintercept.h
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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
#include <unistd.h>

G_BEGIN_DECLS

#define PTY_FD_INVALID (-1)
#define PTY_INTERCEPT_MAGIC (0x81723647)
#define PTY_IS_INTERCEPT(s) ((s) != NULL && (s)->magic == PTY_INTERCEPT_MAGIC)

typedef int                               pty_fd_t;
typedef struct _pty_intercept_t           pty_intercept_t;
typedef struct _pty_intercept_side_t      pty_intercept_side_t;
typedef void (*pty_intercept_callback_t) (const pty_intercept_t      *intercept,
                                          const pty_intercept_side_t *side,
                                          const guint8               *data,
                                          gsize                       len,
                                          gpointer                    user_data);

struct _pty_intercept_side_t
{
  GIOChannel               *channel;
  guint                     in_watch;
  guint                     out_watch;
  GBytes                   *out_bytes;
  pty_intercept_callback_t  callback;
  gpointer                  callback_data;
};

struct _pty_intercept_t
{
  gsize                magic;
  pty_intercept_side_t master;
  pty_intercept_side_t slave;
};

static inline pty_fd_t
pty_fd_steal (pty_fd_t *fd)
{
  pty_fd_t ret = *fd;
  *fd = -1;
  return ret;
}

static void
pty_fd_clear (pty_fd_t *fd)
{
  if (fd != NULL && *fd != -1)
    {
      int rfd = *fd;
      *fd = -1;
      close (rfd);
    }
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (pty_fd_t, pty_fd_clear)

pty_fd_t pty_intercept_create_master (void);
pty_fd_t pty_intercept_create_slave  (pty_fd_t                  master_fd);
gboolean pty_intercept_init          (pty_intercept_t          *self,
                                      pty_fd_t                  fd,
                                      GMainContext             *main_context);
pty_fd_t pty_intercept_get_fd        (pty_intercept_t          *self);
gboolean pty_intercept_set_size      (pty_intercept_t          *self,
                                      guint                     rows,
                                      guint                     columns);
void     pty_intercept_clear         (pty_intercept_t          *self);
void     pty_intercept_set_callback  (pty_intercept_t          *self,
                                      pty_intercept_side_t     *side,
                                      pty_intercept_callback_t  callback,
                                      gpointer                  user_data);

G_END_DECLS