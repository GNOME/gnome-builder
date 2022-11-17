/* ide-log.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-log"

#include "config.h"

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#ifdef __linux__
# include <sys/types.h>
# include <sys/syscall.h>
#endif

#include <glib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "ide-debug.h"
#include "ide-log-private.h"
#include "ide-macros.h"
#include "ide-private.h"

/**
 * SECTION:ide-log
 * @title: Logging
 * @short_description: Standard logging facilities for Builder
 *
 * This module manages the logging facilities in Builder. It involves
 * formatting the standard output and error logs as well as filtering
 * logs based on their #GLogLevelFlags.
 *
 * Generally speaking, you want to continue using the GLib logging API
 * such as g_debug(), g_warning(), g_message(), or g_error(). These functions
 * will redirect their logging information to this module who will format
 * the log message appropriately.
 *
 * If you are writing code for Builder that is in C, you want to ensure you
 * set the %G_LOG_DOMAIN define at the top of your file (after the license)
 * as such:
 *
 * ## Logging from C
 *
 * |[
 * #define G_LOG_DOMAIN "my-module"
 * ...
 * static void
 * some_function (void)
 * {
 *   g_debug ("Use normal logging facilities");
 * }
 * ]|
 *
 * ## Logging from Python
 *
 * If you are writing an extension to Builder from Python, you may use the
 * helper functions provided by our Ide python module.
 *
 * |[<!-- Language="py" -->
 * from gi.repository import Ide
 *
 * Ide.warning("This is a warning")
 * Ide.debug("This is a debug")
 * Ide.error("This is a fatal error")
 * ]|
 */

typedef const gchar *(*IdeLogLevelStrFunc) (GLogLevelFlags log_level);

static GPtrArray          *channels;
static GLogFunc            last_handler;
static int                 log_verbosity;
static IdeLogLevelStrFunc  log_level_str_func;
static gchar              *domains;
static gboolean            has_domains;

G_LOCK_DEFINE (channels_lock);

/**
 * ide_log_get_thread:
 *
 * Retrieves task id for the current thread. This is only supported on Linux.
 * On other platforms, the current thread pointer is retrieved.
 *
 * Returns: The task id.
 */
static inline gint
ide_log_get_thread (void)
{
#ifdef __linux__
  return (gint) syscall (SYS_gettid);
#else
  return GPOINTER_TO_INT (g_thread_self ());
#endif /* __linux__ */
}

/**
 * ide_log_level_str:
 * @log_level: a #GLogLevelFlags.
 *
 * Retrieves the log level as a string.
 *
 * Returns: A string which shouldn't be modified or freed.
 * Side effects: None.
 */
static const gchar *
ide_log_level_str (GLogLevelFlags log_level)
{
  switch (((gulong)log_level & G_LOG_LEVEL_MASK))
    {
    case G_LOG_LEVEL_ERROR:    return "   ERROR";
    case G_LOG_LEVEL_CRITICAL: return "CRITICAL";
    case G_LOG_LEVEL_WARNING:  return " WARNING";
    case G_LOG_LEVEL_MESSAGE:  return " MESSAGE";
    case G_LOG_LEVEL_INFO:     return "    INFO";
    case G_LOG_LEVEL_DEBUG:    return "   DEBUG";
    case IDE_LOG_LEVEL_TRACE:  return "   TRACE";

    default:
      return " UNKNOWN";
    }
}

static const gchar *
ide_log_level_str_with_color (GLogLevelFlags log_level)
{
  switch (((gulong)log_level & G_LOG_LEVEL_MASK))
    {
    case G_LOG_LEVEL_ERROR:    return "   \033[1;31mERROR\033[0m";
    case G_LOG_LEVEL_CRITICAL: return "\033[1;35mCRITICAL\033[0m";
    case G_LOG_LEVEL_WARNING:  return " \033[1;33mWARNING\033[0m";
    case G_LOG_LEVEL_MESSAGE:  return " \033[1;32mMESSAGE\033[0m";
    case G_LOG_LEVEL_INFO:     return "    \033[1;32mINFO\033[0m";
    case G_LOG_LEVEL_DEBUG:    return "   \033[1;32mDEBUG\033[0m";
    case IDE_LOG_LEVEL_TRACE:  return "   \033[1;36mTRACE\033[0m";

    default:
      return " UNKNOWN";
    }
}

/**
 * ide_log_write_to_channel:
 * @channel: a #GIOChannel.
 * @message: A string log message.
 *
 * Writes @message to @channel and flushes the channel.
 */
static void
ide_log_write_to_channel (GIOChannel  *channel,
                          const gchar *message)
{
  g_io_channel_write_chars (channel, message, -1, NULL, NULL);
  g_io_channel_flush (channel, NULL);
}

/**
 * ide_log_handler:
 * @log_domain: A string containing the log section.
 * @log_level: a #GLogLevelFlags.
 * @message: The string message.
 * @user_data: User data supplied to g_log_set_default_handler().
 *
 * Default log handler that will dispatch log messages to configured logging
 * destinations.
 */
static void
ide_log_handler (const gchar    *log_domain,
                 GLogLevelFlags  log_level,
                 const gchar    *message,
                 gpointer        user_data)
{
  gint64 now;
  struct tm tt;
  time_t t;
  const gchar *level;
  gchar ftime[32];
  gchar *buffer;
  gboolean is_debug_level;

  /* Ignore GdkPixbuf chatty-ness */
  if (g_strcmp0 ("GdkPixbuf", log_domain) == 0)
    return;

  /* Let tracer know about log message */
  if ((log_level & G_LOG_LEVEL_MASK) < IDE_LOG_LEVEL_TRACE)
    _ide_trace_log (log_level, log_domain, message);

  if (G_LIKELY (channels->len))
    {
      is_debug_level = (log_level == G_LOG_LEVEL_DEBUG || log_level == IDE_LOG_LEVEL_TRACE);
      if (is_debug_level &&
          has_domains &&
          (log_domain == NULL || strstr (domains, log_domain) == NULL))
        return;

      switch ((int)log_level)
        {
        case G_LOG_LEVEL_MESSAGE:
          if (log_verbosity < 1)
            return;
          break;

        case G_LOG_LEVEL_INFO:
          if (log_verbosity < 2)
            return;
          break;

        case G_LOG_LEVEL_DEBUG:
          if (log_verbosity < 3)
            return;
          break;

        case IDE_LOG_LEVEL_TRACE:
          if (log_verbosity < 4)
            return;
          break;

        default:
          break;
        }

      level = log_level_str_func (log_level);
      now = g_get_real_time ();
      t = now / G_USEC_PER_SEC;
      tt = *localtime (&t);
      strftime (ftime, sizeof (ftime), "%H:%M:%S", &tt);
      buffer = g_strdup_printf ("%s.%04d  %40s[% 5d]: %s: %s\n",
                                ftime,
                                (gint)((now % G_USEC_PER_SEC) / 100L),
                                log_domain,
                                ide_log_get_thread (),
                                level,
                                message);
      G_LOCK (channels_lock);
      g_ptr_array_foreach (channels, (GFunc) ide_log_write_to_channel, buffer);
      G_UNLOCK (channels_lock);
      g_free (buffer);
    }
}

/**
 * ide_log_init:
 * @stdout_: Indicates logging should be written to stdout.
 * @filename: An optional file in which to store logs.
 * @messages_debug: the value of G_MESSAGES_DEBUG environment variable
 *
 * Initializes the logging subsystem. This should be called from
 * the application entry point only. Secondary calls to this function
 * will do nothing.
 */
void
ide_log_init (gboolean    stdout_,
              const char *filename,
              const char *messages_debug)
{
  static gsize initialized = FALSE;
  GIOChannel *channel;

  if (g_once_init_enter (&initialized))
    {
      log_level_str_func = ide_log_level_str;
      channels = g_ptr_array_new ();

      if (filename)
        {
          channel = g_io_channel_new_file (filename, "a", NULL);
          g_ptr_array_add (channels, channel);
        }

      if (stdout_)
        {
          channel = g_io_channel_unix_new (STDOUT_FILENO);
          g_ptr_array_add (channels, channel);
          if ((filename == NULL) && isatty (STDOUT_FILENO))
            log_level_str_func = ide_log_level_str_with_color;
        }

      /* Assume tracing if G_MESSAGES_DEBUG=all */
      if (g_strcmp0 (messages_debug, "all") == 0)
        log_verbosity = 4;

      domains = g_strdup (messages_debug);
      if (!ide_str_empty0 (domains) && strcmp (domains, "all") != 0)
        has_domains = TRUE;

      g_log_set_default_handler (ide_log_handler, NULL);
      g_once_init_leave (&initialized, TRUE);
    }
}

/**
 * ide_log_shutdown:
 *
 * Cleans up after the logging subsystem and restores the original
 * log handler.
 */
void
ide_log_shutdown (void)
{
  if (last_handler)
    {
      g_log_set_default_handler (last_handler, NULL);
      last_handler = NULL;
    }

  g_clear_pointer (&domains, g_free);
}

/**
 * ide_log_increase_verbosity:
 *
 * Increases the amount of logging that will occur. By default, only
 * warning and above will be displayed.
 *
 * Calling this once will cause %G_LOG_LEVEL_MESSAGE to be displayed.
 * Calling this twice will cause %G_LOG_LEVEL_INFO to be displayed.
 * Calling this thrice will cause %G_LOG_LEVEL_DEBUG to be displayed.
 * Calling this four times will cause %IDE_LOG_LEVEL_TRACE to be displayed.
 *
 * Note that many DEBUG and TRACE level log messages are only compiled into
 * debug builds, and therefore will not be available in release builds.
 *
 * This method is meant to be called for every -v provided on the command
 * line.
 *
 * Calling this method more than four times is acceptable.
 */
void
ide_log_increase_verbosity (void)
{
  log_verbosity++;
}

/**
 * ide_log_get_verbosity:
 *
 * Retrieves the log verbosity, which is the number of times -v was
 * provided on the command line.
 */
int
ide_log_get_verbosity (void)
{
  return log_verbosity;
}

/**
 * ide_log_set_verbosity:
 *
 * Sets the explicit verbosity. Generally you want to use
 * ide_log_increase_verbosity() instead of this function.
 */
void
ide_log_set_verbosity (gint level)
{
  log_verbosity = level;
}
