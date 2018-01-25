/* ptyintercept.c
 *
 * Copyright (C) 2018 Christian Hergert
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

#include "ptyintercept.h"

/*
 * We really don't need all that much. A PTY on Linux has a some amount of
 * kernel memory that is non-pageable and therefore small in size. 4k is what
 * it appears to be. Anything more than that is really just an opportunity for
 * us to break some deadlock scenarios.
 */
#define CHANNEL_BUFFER_SIZE (4096 * 4)


static void     _pty_intercept_side_close (pty_intercept_side_t *side);
static gboolean _pty_intercept_in_cb      (GIOChannel           *channel,
                                           GIOCondition          condition,
                                           gpointer              user_data);
static gboolean _pty_intercept_out_cb     (GIOChannel           *channel,
                                           GIOCondition          condition,
                                           gpointer              user_data);
static void     clear_source              (guint                *source_id);

static gboolean
_pty_intercept_set_raw (pty_fd_t fd)
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
 * pty_intercept_create_slave:
 * @master_fd: a pty master
 *
 * This creates a new slave to the PTY master @master_fd.
 *
 * This uses grantpt(), unlockpt(), and ptsname() to open a new
 * PTY slave.
 *
 * Returns: a FD for the slave PTY that should be closed with close().
 *   Upon error, %PTY_FD_INVALID (-1) is returned.
 */
pty_fd_t
pty_intercept_create_slave (pty_fd_t master_fd)
{
  g_auto(pty_fd_t) ret = PTY_FD_INVALID;
#ifdef HAVE_PTSNAME_R
  char name[256];
#else
  const char *name;
#endif

  g_assert (master_fd != -1);

  if (grantpt (master_fd) != 0)
    return PTY_FD_INVALID;

  if (unlockpt (master_fd) != 0)
    return PTY_FD_INVALID;

#ifdef HAVE_PTSNAME_R
  if (ptsname_r (master_fd, name, sizeof name - 1) != 0)
    return PTY_FD_INVALID;
  name[sizeof name - 1] = '\0';
#else
  if (NULL == (name = ptsname (master_fd)))
    return PTY_FD_INVALID;
#endif

  ret =  open (name, O_RDWR | O_CLOEXEC | O_NONBLOCK);

  if (ret == PTY_FD_INVALID && errno == EINVAL)
    {
      gint flags;

      ret = open (name, O_RDWR | O_CLOEXEC);
      if (ret == PTY_FD_INVALID && errno == EINVAL)
        ret = open (name, O_RDWR);

      if (ret == PTY_FD_INVALID)
        return PTY_FD_INVALID;

      /* Add FD_CLOEXEC if O_CLOEXEC failed */
      flags = fcntl (ret, F_GETFD, 0);
      if ((flags & FD_CLOEXEC) == 0)
        {
          if (fcntl (ret, F_SETFD, flags | FD_CLOEXEC) < 0)
            return PTY_FD_INVALID;
        }

      if (!g_unix_set_fd_nonblocking (ret, TRUE, NULL))
        return PTY_FD_INVALID;
    }

  return pty_fd_steal (&ret);
}

/**
 * pty_intercept_create_master:
 *
 * Creates a new PTY master using posix_openpt(). Some fallbacks are
 * provided for non-Linux systems where O_CLOEXEC and O_NONBLOCK may
 * not be supported.
 *
 * Returns: a FD that should be closed with close() if successful.
 *   Upon error, %PTY_FD_INVALID (-1) is returned.
 */
pty_fd_t
pty_intercept_create_master (void)
{
  g_auto(pty_fd_t) master_fd = PTY_FD_INVALID;

  master_fd = posix_openpt (O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);

#ifndef __linux__
  /* Fallback for operating systems that don't support
   * O_NONBLOCK and O_CLOEXEC when opening.
   */
  if (master_fd == PTY_FD_INVALID && errno == EINVAL)
    {
      gint new_flags = O_NONBLOCK;
      gint flags;

      master_fd = posix_openpt (O_RDWR | O_NOCTTY | O_CLOEXEC);

      if (master_fd == PTY_FD_INVALID && errno == EINVAL)
        {
          master_fd = posix_openpt (O_RDWR | O_NOCTTY);
          new_flags |= FD_CLOEXEC;
          if (master_fd == -1)
            return PTY_FD_INVALID;
        }

      flags = fcntl (master_fd, F_GETFD, 0);
      if (flags < 0)
        return PTY_FD_INVALID;

      if (fcntl (master_fd, F_SETFD, flags | new_flags) < 0)
        return PTY_FD_INVALID;
    }
#endif

  return pty_fd_steal (&master_fd);
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
_pty_intercept_side_close (pty_intercept_side_t *side)
{
  g_assert (side != NULL);

  clear_source (&side->in_watch);
  clear_source (&side->out_watch);
  g_clear_pointer (&side->channel, g_io_channel_unref);
  g_clear_pointer (&side->out_bytes, g_bytes_unref);
}

static gboolean
_pty_intercept_out_cb (GIOChannel   *channel,
                       GIOCondition  condition,
                       gpointer      user_data)
{
  pty_intercept_t *self = user_data;
  pty_intercept_side_t *us, *them;
  GIOStatus status;
  const gchar *wrbuf;
  gsize n_written = 0;
  gsize len = 0;

  g_assert (channel != NULL);
  g_assert (condition & (G_IO_ERR | G_IO_HUP | G_IO_OUT));

  if (channel == self->master.channel)
    {
      us = &self->master;
      them = &self->slave;
    }
  else
    {
      us = &self->slave;
      them = &self->master;
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
                         G_PRIORITY_DEFAULT,
                         G_IO_IN | G_IO_ERR | G_IO_HUP,
                         _pty_intercept_in_cb,
                         self, NULL);

  return G_SOURCE_REMOVE;

close_and_cleanup:

  _pty_intercept_side_close (us);
  _pty_intercept_side_close (them);

  return G_SOURCE_REMOVE;
}

/*
 * _pty_intercept_in_cb:
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
_pty_intercept_in_cb (GIOChannel   *channel,
                      GIOCondition  condition,
                      gpointer      user_data)
{
  pty_intercept_t *self = user_data;
  pty_intercept_side_t *us, *them;
  GIOStatus status = G_IO_STATUS_AGAIN;
  gchar buf[4096];
  gchar *wrbuf = buf;
  gsize n_read;

  g_assert (channel != NULL);
  g_assert (condition & (G_IO_ERR | G_IO_HUP | G_IO_IN));
  g_assert (PTY_IS_INTERCEPT (self));

  if (channel == self->master.channel)
    {
      us = &self->master;
      them = &self->slave;
    }
  else
    {
      us = &self->slave;
      them = &self->master;
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
                                                 G_PRIORITY_DEFAULT,
                                                 G_IO_OUT | G_IO_ERR | G_IO_HUP,
                                                 _pty_intercept_out_cb,
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

  _pty_intercept_side_close (us);
  _pty_intercept_side_close (them);

  return G_SOURCE_REMOVE;
}

/**
 * pty_intercept_set_size:
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
pty_intercept_set_size (pty_intercept_t *self,
                        guint            rows,
                        guint            columns)
{

  g_return_val_if_fail (PTY_IS_INTERCEPT (self), FALSE);

  if (self->master.channel != NULL)
    {
      pty_fd_t fd = g_io_channel_unix_get_fd (self->master.channel);
      struct winsize ws = {0};

      ws.ws_col = columns;
      ws.ws_row = rows;

      return ioctl (fd, TIOCSWINSZ, &ws) == 0;
    }

  return FALSE;
}

/**
 * pty_intercept_init:
 * @self: a location of memory to store a #pty_intercept_t
 * @fd: the PTY master fd, possibly from a #VtePty
 * @main_context: (nullable): a #GMainContext or %NULL for thread-default
 *
 * Creates a enw #pty_intercept_t using the PTY master fd @fd.
 *
 * A new PTY slave is created that will communicate with @fd.
 * Additionally, a new PTY master is created that can communicate
 * with another side, and will pass that information to @fd after
 * extracting any necessary information.
 *
 * Returns: %TRUE if successful; otherwise %FALSE
 */
gboolean
pty_intercept_init (pty_intercept_t *self,
                    int              fd,
                    GMainContext    *main_context)
{
  g_auto(pty_fd_t) slave_fd = PTY_FD_INVALID;
  g_auto(pty_fd_t) master_fd = PTY_FD_INVALID;
  struct winsize ws;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (fd != -1, FALSE);

  memset (self, 0, sizeof *self);
  self->magic = PTY_INTERCEPT_MAGIC;

  slave_fd = pty_intercept_create_slave (fd);
  if (slave_fd == PTY_FD_INVALID)
    return FALSE;

  /* Do not perform additional processing on the slave_fd created
   * from the master we were provided. Otherwise, it will be happening
   * twice instead of just once.
   */
  if (!_pty_intercept_set_raw (slave_fd))
    return FALSE;

  master_fd = pty_intercept_create_master ();
  if (master_fd == PTY_FD_INVALID)
    return FALSE;

  /* Copy the win size across */
  if (ioctl (slave_fd, TIOCGWINSZ, &ws) >= 0)
    ioctl (master_fd, TIOCSWINSZ, &ws);

  if (main_context == NULL)
    main_context = g_main_context_get_thread_default ();

  self->master.channel = g_io_channel_unix_new (pty_fd_steal (&master_fd));
  self->slave.channel = g_io_channel_unix_new (pty_fd_steal (&slave_fd));

  g_io_channel_set_close_on_unref (self->master.channel, TRUE);
  g_io_channel_set_close_on_unref (self->slave.channel, TRUE);

  g_io_channel_set_encoding (self->master.channel, NULL, NULL);
  g_io_channel_set_encoding (self->slave.channel, NULL, NULL);

  g_io_channel_set_buffer_size (self->master.channel, CHANNEL_BUFFER_SIZE);
  g_io_channel_set_buffer_size (self->slave.channel, CHANNEL_BUFFER_SIZE);

  self->master.in_watch =
    g_io_add_watch_full (self->master.channel,
                         G_PRIORITY_DEFAULT,
                         G_IO_IN | G_IO_ERR | G_IO_HUP,
                         _pty_intercept_in_cb,
                         self, NULL);

  self->slave.in_watch =
    g_io_add_watch_full (self->slave.channel,
                         G_PRIORITY_DEFAULT,
                         G_IO_IN | G_IO_ERR | G_IO_HUP,
                         _pty_intercept_in_cb,
                         self, NULL);

  return TRUE;
}

/**
 * pty_intercept_clear:
 * @self: a #pty_intercept_t
 *
 * Cleans up a #pty_intercept_t previously initialized with
 * pty_intercept_init().
 *
 * This diconnects any #GIOChannel that have been attached and
 * releases any allocated memory.
 *
 * It is invalid to use @self after calling this function.
 */
void
pty_intercept_clear (pty_intercept_t *self)
{
  g_return_if_fail (PTY_IS_INTERCEPT (self));

  clear_source (&self->slave.in_watch);
  clear_source (&self->slave.out_watch);
  g_clear_pointer (&self->slave.channel, g_io_channel_unref);
  g_clear_pointer (&self->slave.out_bytes, g_bytes_unref);

  clear_source (&self->master.in_watch);
  clear_source (&self->master.out_watch);
  g_clear_pointer (&self->master.channel, g_io_channel_unref);
  g_clear_pointer (&self->master.out_bytes, g_bytes_unref);

  memset (self, 0, sizeof *self);
}

/**
 * pty_intercept_get_fd:
 * @self: a #pty_intercept_t
 *
 * Gets a master PTY fd created by the #pty_intercept_t. This is suitable
 * to use to create a slave fd which can be passed to a child process.
 *
 * Returns: A FD of a PTY master if successful, otherwise -1.
 */
pty_fd_t
pty_intercept_get_fd (pty_intercept_t *self)
{
  g_return_val_if_fail (PTY_IS_INTERCEPT (self), PTY_FD_INVALID);
  g_return_val_if_fail (self->master.channel != NULL, PTY_FD_INVALID);

  return g_io_channel_unix_get_fd (self->master.channel);
}

/**
 * pty_intercept_set_callback:
 * @self: a pty_intercept_t
 * @side: the side containing the data to watch
 * @callback: the callback to execute when data is received
 * @user_data: closure data for @callback
 *
 * This sets the callback to execute every time data is received
 * from a particular side of the intercept.
 *
 * You may only set one per side.
 */
void
pty_intercept_set_callback (pty_intercept_t          *self,
                            pty_intercept_side_t     *side,
                            pty_intercept_callback_t  callback,
                            gpointer                  callback_data)
{
  g_return_if_fail (PTY_IS_INTERCEPT (self));
  g_return_if_fail (side == &self->master || side == &self->slave);

  side->callback = callback;
  side->callback_data = callback_data;
}
