/* bug-buddy.c
 *
 * Copyright 2017-2019 Christian Hergert <christian@hergert.me>
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

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <glib.h>

#include "bug-buddy.h"

/*
 * This is not the bug buddy you're looking for. It's just named after GNOME's
 * ancient Bug Buddy.
 *
 * This file sets up the necessary state at startup and then executes gdb from
 * a SIGSEGV handler so that we get a useful stack trace when the process
 * unexpectedly exits.
 */

static gchar **gdb_argv = NULL;

G_GNUC_NORETURN static void
bug_buddy_sigsegv_handler (int signum)
{
  int pid;

  pid = fork ();

  if (pid == 0)
    {
      execv (gdb_argv[0], gdb_argv);
    }
  else
    {
      waitpid (pid, NULL, 0);
    }

  _exit (-1);
}

void
bug_buddy_init (void)
{
  gchar *gdb_path;
  GPtrArray *argv;

  /*
   * Everything needs to be prepared at startup so that we can avoid using
   * any malloc, locks, etc in our SIGSEGV handler. So we'll find gdb right
   * now and stash the location for later. If it disappears during runtime,
   * that's fine, we just won't be able to invoke gdb.
   */

  gdb_path = g_find_program_in_path ("gdb");
  if (gdb_path == NULL)
    return;

  argv = g_ptr_array_sized_new (12);
  g_ptr_array_add (argv, gdb_path);
  g_ptr_array_add (argv, (gchar *)"-batch");
  g_ptr_array_add (argv, (gchar *)"-nx");
  g_ptr_array_add (argv, (gchar *)"-ex");
  g_ptr_array_add (argv, g_strdup_printf ("attach %"G_PID_FORMAT, getpid ()));
  g_ptr_array_add (argv, (gchar *)"-ex");
  g_ptr_array_add (argv, (gchar *)"info threads");
  g_ptr_array_add (argv, (gchar *)"-ex");
  g_ptr_array_add (argv, (gchar *)"thread apply all bt");
  g_ptr_array_add (argv, (gchar *)"-ex");
  g_ptr_array_add (argv, (gchar *)"info sharedlibrary");
  g_ptr_array_add (argv, NULL);
  gdb_argv = (gchar **)g_ptr_array_free (argv, FALSE);

  /*
   * Now register our signal handler so that we get called on SIGSEGV.
   * We'll use that signal callback to extract the backtrace with gdb.
   */
  signal (SIGSEGV, bug_buddy_sigsegv_handler);
}
