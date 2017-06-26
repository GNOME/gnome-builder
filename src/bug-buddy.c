/* bug-buddy.c
 *
 * Copyright (C) 2017 Christian Hergert <christian@hergert.me>
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

#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "bug-buddy.h"

/*
 * This is not the bug buddy you're looking for. It's just named after GNOME's
 * ancient Bug Buddy.
 *
 * This file sets up the necessary state at startup and then executes gdb from
 * a SIGSEGV handler so that we get a useful stack trace when the process
 * unexpectedly exits.
 */

static struct
{
  /* Our stashed path to the GDB binary */
  gchar gdb_path[1024];
  gchar commands[1024];
} bug_buddy_state;

static void
bug_buddy_sigsegv_handler (int signum)
{
  gchar gdb_filename[] = "/tmp/gnome-builder-gdb-commands.XXXXXX";
  gchar *argv[8] = { NULL };
  GPid pid;
  int fd = -1;
  int status;

  /* Only proceed if we have a gdb path to execute */
  if (bug_buddy_state.gdb_path[0] == '\0')
    goto failure;

  if (-1 == (fd = g_mkstemp (gdb_filename)))
    goto failure;

  /* Call once, hope for the best. */
  write (fd, bug_buddy_state.commands, strlen (bug_buddy_state.commands));
  fsync (fd);

  argv[0] = bug_buddy_state.gdb_path;
  argv[1] = "-batch";
  argv[2] = "-x";
  argv[3] = gdb_filename;
  argv[4] = "-nx";

  close (fd);
  fd = -1;

  pid = fork ();

  if (pid == 0)
    {
      execv (argv[0], (gchar **)argv);
    }
  else
    {
      waitpid (pid, &status, 0);
      unlink (gdb_filename);
    }

failure:

  _exit (-1);
}

void
bug_buddy_init (void)
{
  GString *str = NULL;
  gchar *gdb_path = NULL;

  /*
   * Everything needs to be prepared at startup so that we can avoid using
   * any malloc, locks, etc in our SIGSEGV handler. So we'll find gdb right
   * now and stash the location for later. If it disappears during runtime,
   * that's fine, we just wont be able to invoke gdb.
   */

  gdb_path = g_find_program_in_path ("gdb");
  if (strlen (gdb_path) < ((sizeof bug_buddy_state.gdb_path) - 1))
    g_strlcpy (bug_buddy_state.gdb_path, gdb_path, sizeof bug_buddy_state.gdb_path);
  else
    goto cleanup;

  /*
   * Build our commands list. Since we know our process up front, we can just
   * use getpid() to prepare the commands now.
   */
  str = g_string_new (NULL);
  g_string_append_printf (str, "attach %"G_PID_FORMAT"\n", getpid ());
  g_string_append (str, "info threads\n");
  g_string_append (str, "thread apply all bt\n");
  g_string_append (str, "info sharedlibrary\n");
  g_assert (str->len < sizeof bug_buddy_state.commands);
  g_strlcpy (bug_buddy_state.commands, str->str, sizeof bug_buddy_state.commands);

  /*
   * Now register our signal handler so that we get called on SIGSEGV.
   * We'll use that signal callback to extract the backtrace with gdb.
   */
  signal (SIGSEGV, bug_buddy_sigsegv_handler);

cleanup:
  g_free (gdb_path);
  if (str != NULL)
    g_string_free (str, TRUE);

}
