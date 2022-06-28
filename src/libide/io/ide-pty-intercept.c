/* ide-pty-intercept.c
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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <glib-unix.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "ide-pty-intercept.h"

/*
 * We really don't need all that much. A PTY on Linux has a some amount of
 * kernel memory that is non-pageable and therefore small in size. 4k is what
 * it appears to be. Anything more than that is really just an opportunity for
 * us to break some deadlock scenarios.
 */
#define CHANNEL_BUFFER_SIZE (4096 * 4)
#define SLAVE_READ_PRIORITY   G_PRIORITY_HIGH
#define SLAVE_WRITE_PRIORITY  G_PRIORITY_DEFAULT_IDLE
#define MASTER_READ_PRIORITY  G_PRIORITY_DEFAULT_IDLE
#define MASTER_WRITE_PRIORITY G_PRIORITY_HIGH

static void     _ide_pty_intercept_side_close (IdePtyInterceptSide *side);
static gboolean _ide_pty_intercept_in_cb      (GIOChannel          *channel,
                                               GIOCondition         condition,
                                               gpointer             user_data);
static gboolean _ide_pty_intercept_out_cb     (GIOChannel          *channel,
                                               GIOCondition         condition,
                                               gpointer             user_data);
static void     clear_source                  (guint               *source_id);

static gboolean
_ide_pty_intercept_set_raw (IdePtyFd fd)
{
  struct termios t;

  if (tcgetattr (fd, &t) == -1)
    return FALSE;

  t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
  t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR | INPCK | ISTRIP | IXON | PARMRK);
  t.c_oflag &= ~(OPOST);
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;

  if (tcsetattr (fd, TCSAFLUSH, &t) == -1)
    return FALSE;

  return TRUE;
}

/**
 * ide_pty_intercept_create_producer:
 * @consumer_fd: a pty
 * @blocking: use %FALSE to set O_NONBLOCK
 *
 * This creates a new producer to the PTY consumer @consumer_fd.
 *
 * This uses grantpt(), unlockpt(), and ptsname() to open a new
 * PTY producer.
 *
 * Returns: a FD for the producer PTY that should be closed with close().
 *   Upon error, %IDE_PTY_FD_INVALID (-1) is returned.
 */
IdePtyFd
ide_pty_intercept_create_producer (IdePtyFd consumer_fd,
                                   gboolean blocking)
{
  g_auto(IdePtyFd) ret = IDE_PTY_FD_INVALID;
  gint extra = blocking ? 0 : O_NONBLOCK;
#if defined(HAVE_PTSNAME_R) || defined(__FreeBSD__)
  char name[256];
#else
  const char *name;
#endif

  g_assert (consumer_fd != -1);

  if (grantpt (consumer_fd) != 0)
    return IDE_PTY_FD_INVALID;

  if (unlockpt (consumer_fd) != 0)
    return IDE_PTY_FD_INVALID;

#ifdef HAVE_PTSNAME_R
  if (ptsname_r (consumer_fd, name, sizeof name - 1) != 0)
    return IDE_PTY_FD_INVALID;
  name[sizeof name - 1] = '\0';
#elif defined(__FreeBSD__)
  if (fdevname_r (consumer_fd, name + 5, sizeof name - 6) == NULL)
    return IDE_PTY_FD_INVALID;
  memcpy (name, "/dev/", 5);
  name[sizeof name - 1] = '\0';
#else
  if (NULL == (name = ptsname (consumer_fd)))
    return IDE_PTY_FD_INVALID;
#endif

  ret = open (name, O_NOCTTY | O_RDWR | O_CLOEXEC | extra);

  if (ret == IDE_PTY_FD_INVALID && errno == EINVAL)
    {
      gint flags;

      ret = open (name, O_NOCTTY | O_RDWR | O_CLOEXEC);
      if (ret == IDE_PTY_FD_INVALID && errno == EINVAL)
        ret = open (name, O_NOCTTY | O_RDWR);

      if (ret == IDE_PTY_FD_INVALID)
        return IDE_PTY_FD_INVALID;

      /* Add FD_CLOEXEC if O_CLOEXEC failed */
      flags = fcntl (ret, F_GETFD, 0);
      if ((flags & FD_CLOEXEC) == 0)
        {
          if (fcntl (ret, F_SETFD, flags | FD_CLOEXEC) < 0)
            return IDE_PTY_FD_INVALID;
        }

      if (!blocking)
        {
          if (!g_unix_set_fd_nonblocking (ret, TRUE, NULL))
            return IDE_PTY_FD_INVALID;
        }
    }

  return pty_fd_steal (&ret);
}

/**
 * ide_pty_intercept_create_consumer:
 *
 * Creates a new PTY consumer using posix_openpt(). Some fallbacks are
 * provided for non-Linux systems where O_CLOEXEC and O_NONBLOCK may
 * not be supported.
 *
 * Returns: a FD that should be closed with close() if successful.
 *   Upon error, %IDE_PTY_FD_INVALID (-1) is returned.
 */
IdePtyFd
ide_pty_intercept_create_consumer (void)
{
  g_auto(IdePtyFd) consumer_fd = IDE_PTY_FD_INVALID;

  consumer_fd = posix_openpt (O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);

#ifndef __linux__
  /* Fallback for operating systems that don't support
   * O_NONBLOCK and O_CLOEXEC when opening.
   */
  if (consumer_fd == IDE_PTY_FD_INVALID && errno == EINVAL)
    {
      consumer_fd = posix_openpt (O_RDWR | O_NOCTTY | O_CLOEXEC);

      if (consumer_fd == IDE_PTY_FD_INVALID && errno == EINVAL)
        {
          gint flags;

          consumer_fd = posix_openpt (O_RDWR | O_NOCTTY);
          if (consumer_fd == -1)
            return IDE_PTY_FD_INVALID;

          flags = fcntl (consumer_fd, F_GETFD, 0);
          if (flags < 0)
            return IDE_PTY_FD_INVALID;

          if (fcntl (consumer_fd, F_SETFD, flags | FD_CLOEXEC) < 0)
            return IDE_PTY_FD_INVALID;
        }

      if (!g_unix_set_fd_nonblocking (consumer_fd, TRUE, NULL))
        return IDE_PTY_FD_INVALID;
    }
#endif

  return pty_fd_steal (&consumer_fd);
}

static void
clear_source (guint *source_id)
{
  guint id = *source_id;
  *source_id = 0;
  if (id != 0)
    g_source_remove (id);
}

static void
_ide_pty_intercept_side_close (IdePtyInterceptSide *side)
{
  g_assert (side != NULL);

  clear_source (&side->in_watch);
  clear_source (&side->out_watch);
  g_clear_pointer (&side->channel, g_io_channel_unref);
  g_clear_pointer (&side->out_bytes, g_bytes_unref);
}

static gboolean
_ide_pty_intercept_out_cb (GIOChannel   *channel,
                           GIOCondition  condition,
                           gpointer      user_data)
{
  IdePtyIntercept *self = user_data;
  IdePtyInterceptSide *us, *them;
  GIOStatus status;
  const gchar *wrbuf;
  gsize n_written = 0;
  gsize len = 0;

  g_assert (channel != NULL);
  g_assert (condition & (G_IO_ERR | G_IO_HUP | G_IO_OUT));

  if (channel == self->consumer.channel)
    {
      us = &self->consumer;
      them = &self->producer;
    }
  else
    {
      us = &self->producer;
      them = &self->consumer;
    }

  if ((condition & G_IO_OUT) == 0 ||
      us->out_bytes == NULL ||
      us->channel == NULL ||
      them->channel == NULL)
    goto close_and_cleanup;

  wrbuf = g_bytes_get_data (us->out_bytes, &len);
  status = g_io_channel_write_chars (us->channel, wrbuf, len, &n_written, NULL);
  if (status != G_IO_STATUS_NORMAL)
    goto close_and_cleanup;

  g_assert (n_written > 0);
  g_assert (them->in_watch == 0);

  /*
   * If we didn't write all of our data, wait until another G_IO_OUT
   * condition to write more data.
   */
  if (n_written < len)
    {
      g_autoptr(GBytes) bytes = g_steal_pointer (&us->out_bytes);
      us->out_bytes = g_bytes_new_from_bytes (bytes, n_written, len - n_written);
      return G_SOURCE_CONTINUE;
    }

  g_clear_pointer (&us->out_bytes, g_bytes_unref);

  /*
   * We wrote all the data to this side, so now we can wait for more
   * data from the input peer.
   */
  us->out_watch = 0;
  them->in_watch =
    g_io_add_watch_full (them->channel,
                         them->read_prio,
                         G_IO_IN | G_IO_ERR | G_IO_HUP,
                         _ide_pty_intercept_in_cb,
                         self, NULL);

  return G_SOURCE_REMOVE;

close_and_cleanup:

  _ide_pty_intercept_side_close (us);
  _ide_pty_intercept_side_close (them);

  return G_SOURCE_REMOVE;
}

/*
 * _ide_pty_intercept_in_cb:
 *
 * This function is called when we have received a condition that specifies
 * the channel has data to read. We read that data and then setup a watch
 * onto the other other side so that we can write that data.
 *
 * If the other-side of the of the connection can write, then we write
 * that data immediately.
 *
 * The in watch is disabled until we have completed the write.
 */
static gboolean
_ide_pty_intercept_in_cb (GIOChannel   *channel,
                          GIOCondition  condition,
                          gpointer      user_data)
{
  IdePtyIntercept *self = user_data;
  IdePtyInterceptSide *us, *them;
  GIOStatus status = G_IO_STATUS_AGAIN;
  gchar buf[4096];
  gchar *wrbuf = buf;
  gsize n_read;

  g_assert (channel != NULL);
  g_assert (condition & (G_IO_ERR | G_IO_HUP | G_IO_IN));
  g_assert (IDE_IS_PTY_INTERCEPT (self));

  if (channel == self->consumer.channel)
    {
      us = &self->consumer;
      them = &self->producer;
    }
  else
    {
      us = &self->producer;
      them = &self->consumer;
    }

  g_assert (us->in_watch != 0);
  g_assert (them->out_watch == 0);

  if (condition & (G_IO_ERR | G_IO_HUP) || us->channel == NULL || them->channel == NULL)
    goto close_and_cleanup;

  g_assert (condition & G_IO_IN);

  while (status == G_IO_STATUS_AGAIN)
    {
      n_read = 0;
      status = g_io_channel_read_chars (us->channel, buf, sizeof buf, &n_read, NULL);
    }

  if (status == G_IO_STATUS_EOF)
    goto close_and_cleanup;

  if (n_read > 0 && us->callback != NULL)
    us->callback (self, us, (const guint8 *)buf, n_read, us->callback_data);

  while (n_read > 0)
    {
      gsize n_written = 0;

      status = g_io_channel_write_chars (them->channel, buf, n_read, &n_written, NULL);

      wrbuf += n_written;
      n_read -= n_written;

      if (n_read > 0 && status == G_IO_STATUS_AGAIN)
        {
          /* If we get G_IO_STATUS_AGAIN here, then we are in a situation where
           * the other side is not in a position to handle the data. We need to
           * setup a G_IO_OUT watch on the FD to wait until things are writeable.
           *
           * We'll cancel our G_IO_IN condition, and wait for the out condition
           * to make forward progress.
           */
          them->out_bytes = g_bytes_new (wrbuf, n_read);
          them->out_watch = g_io_add_watch_full (them->channel,
                                                 them->write_prio,
                                                 G_IO_OUT | G_IO_ERR | G_IO_HUP,
                                                 _ide_pty_intercept_out_cb,
                                                 self, NULL);
          us->in_watch = 0;

          return G_SOURCE_REMOVE;
        }

      if (status != G_IO_STATUS_NORMAL)
        goto close_and_cleanup;

      g_io_channel_flush (them->channel, NULL);
    }

  return G_SOURCE_CONTINUE;

close_and_cleanup:

  _ide_pty_intercept_side_close (us);
  _ide_pty_intercept_side_close (them);

  return G_SOURCE_REMOVE;
}

/**
 * ide_pty_intercept_set_size:
 *
 * Proxies a winsize across to the inferior. If the PTY is the
 * controlling PTY for the process, then SIGWINCH will be signaled
 * in the inferior process.
 *
 * Since we can't track SIGWINCH cleanly in here, we rely on the
 * external consuming program to notify us of SIGWINCH so that we
 * can copy the new size across.
 */
gboolean
ide_pty_intercept_set_size (IdePtyIntercept *self,
                            guint            rows,
                            guint            columns)
{

  g_return_val_if_fail (IDE_IS_PTY_INTERCEPT (self), FALSE);

  if (self->consumer.channel != NULL)
    {
      IdePtyFd fd = g_io_channel_unix_get_fd (self->consumer.channel);
      struct winsize ws = {0};

      ws.ws_col = columns;
      ws.ws_row = rows;

      return ioctl (fd, TIOCSWINSZ, &ws) == 0;
    }

  return FALSE;
}

static guint
_g_io_add_watch_full_with_context (GMainContext   *main_context,
                                   GIOChannel     *channel,
                                   gint            priority,
                                   GIOCondition    condition,
                                   GIOFunc         func,
                                   gpointer        user_data,
                                   GDestroyNotify  notify)
{
  GSource *source;
  guint id;

  g_return_val_if_fail (channel != NULL, 0);

  source = g_io_create_watch (channel, condition);

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);
  g_source_set_callback (source, (GSourceFunc)func, user_data, notify);

  id = g_source_attach (source, main_context);
  g_source_unref (source);

  return id;
}

/**
 * ide_pty_intercept_init:
 * @self: a location of memory to store a #IdePtyIntercept
 * @fd: the PTY consumer fd, possibly from a #VtePty
 * @main_context: (nullable): a #GMainContext or %NULL for thread-default
 *
 * Creates a enw #IdePtyIntercept using the PTY consumer fd @fd.
 *
 * A new PTY producer is created that will communicate with @fd.
 * Additionally, a new PTY consumer is created that can communicate
 * with another side, and will pass that information to @fd after
 * extracting any necessary information.
 *
 * Returns: %TRUE if successful; otherwise %FALSE
 */
gboolean
ide_pty_intercept_init (IdePtyIntercept *self,
                        int              fd,
                        GMainContext    *main_context)
{
  g_auto(IdePtyFd) producer_fd = IDE_PTY_FD_INVALID;
  g_auto(IdePtyFd) consumer_fd = IDE_PTY_FD_INVALID;
  struct winsize ws;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (fd != -1, FALSE);

  memset (self, 0, sizeof *self);
  self->magic = IDE_PTY_INTERCEPT_MAGIC;

  producer_fd = ide_pty_intercept_create_producer (fd, FALSE);
  if (producer_fd == IDE_PTY_FD_INVALID)
    return FALSE;

  /* Do not perform additional processing on the producer_fd created
   * from the consumer we were provided. Otherwise, it will be happening
   * twice instead of just once.
   */
  if (!_ide_pty_intercept_set_raw (producer_fd))
    return FALSE;

  consumer_fd = ide_pty_intercept_create_consumer ();
  if (consumer_fd == IDE_PTY_FD_INVALID)
    return FALSE;

  /* Copy the win size across */
  if (ioctl (producer_fd, TIOCGWINSZ, &ws) >= 0)
    ioctl (consumer_fd, TIOCSWINSZ, &ws);

  if (main_context == NULL)
    main_context = g_main_context_get_thread_default ();

  self->consumer.read_prio = MASTER_READ_PRIORITY;
  self->consumer.write_prio = MASTER_WRITE_PRIORITY;
  self->producer.read_prio = SLAVE_READ_PRIORITY;
  self->producer.write_prio = SLAVE_WRITE_PRIORITY;

  self->consumer.channel = g_io_channel_unix_new (pty_fd_steal (&consumer_fd));
  self->producer.channel = g_io_channel_unix_new (pty_fd_steal (&producer_fd));

  g_io_channel_set_close_on_unref (self->consumer.channel, TRUE);
  g_io_channel_set_close_on_unref (self->producer.channel, TRUE);

  g_io_channel_set_encoding (self->consumer.channel, NULL, NULL);
  g_io_channel_set_encoding (self->producer.channel, NULL, NULL);

  g_io_channel_set_buffer_size (self->consumer.channel, CHANNEL_BUFFER_SIZE);
  g_io_channel_set_buffer_size (self->producer.channel, CHANNEL_BUFFER_SIZE);

  self->consumer.in_watch =
    _g_io_add_watch_full_with_context (main_context,
                                       self->consumer.channel,
                                       self->consumer.read_prio,
                                       G_IO_IN | G_IO_ERR | G_IO_HUP,
                                       _ide_pty_intercept_in_cb,
                                       self, NULL);

  self->producer.in_watch =
    _g_io_add_watch_full_with_context (main_context,
                                       self->producer.channel,
                                       self->producer.read_prio,
                                       G_IO_IN | G_IO_ERR | G_IO_HUP,
                                       _ide_pty_intercept_in_cb,
                                       self, NULL);

  return TRUE;
}

/**
 * ide_pty_intercept_clear:
 * @self: a #IdePtyIntercept
 *
 * Cleans up a #IdePtyIntercept previously initialized with
 * ide_pty_intercept_init().
 *
 * This diconnects any #GIOChannel that have been attached and
 * releases any allocated memory.
 *
 * It is invalid to use @self after calling this function.
 */
void
ide_pty_intercept_clear (IdePtyIntercept *self)
{
  g_return_if_fail (IDE_IS_PTY_INTERCEPT (self));

  clear_source (&self->producer.in_watch);
  clear_source (&self->producer.out_watch);
  g_clear_pointer (&self->producer.channel, g_io_channel_unref);
  g_clear_pointer (&self->producer.out_bytes, g_bytes_unref);

  clear_source (&self->consumer.in_watch);
  clear_source (&self->consumer.out_watch);
  g_clear_pointer (&self->consumer.channel, g_io_channel_unref);
  g_clear_pointer (&self->consumer.out_bytes, g_bytes_unref);

  memset (self, 0, sizeof *self);
}

/**
 * ide_pty_intercept_get_fd:
 * @self: a #IdePtyIntercept
 *
 * Gets a consumer PTY fd created by the #IdePtyIntercept. This is suitable
 * to use to create a producer fd which can be passed to a child process.
 *
 * Returns: A FD of a PTY consumer if successful, otherwise -1.
 */
IdePtyFd
ide_pty_intercept_get_fd (IdePtyIntercept *self)
{
  g_return_val_if_fail (IDE_IS_PTY_INTERCEPT (self), IDE_PTY_FD_INVALID);
  g_return_val_if_fail (self->consumer.channel != NULL, IDE_PTY_FD_INVALID);

  return g_io_channel_unix_get_fd (self->consumer.channel);
}

/**
 * ide_pty_intercept_set_callback:
 * @self: a IdePtyIntercept
 * @side: the side containing the data to watch
 * @callback: (scope notified): the callback to execute when data is received
 * @user_data: closure data for @callback
 *
 * This sets the callback to execute every time data is received
 * from a particular side of the intercept.
 *
 * You may only set one per side.
 */
void
ide_pty_intercept_set_callback (IdePtyIntercept         *self,
                                IdePtyInterceptSide     *side,
                                IdePtyInterceptCallback  callback,
                                gpointer                 callback_data)
{
  g_return_if_fail (IDE_IS_PTY_INTERCEPT (self));
  g_return_if_fail (side == &self->consumer || side == &self->producer);

  side->callback = callback;
  side->callback_data = callback_data;
}
