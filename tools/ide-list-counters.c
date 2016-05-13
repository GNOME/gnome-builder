/* ide-list-counters.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "egg-counter.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void
foreach_cb (EggCounter *counter,
            gpointer    user_data)
{
  guint *n_counters = user_data;

  (*n_counters)++;

  g_print ("%-20s : %-32s : %20"G_GINT64_FORMAT" : %-s\n",
           counter->category,
           counter->name,
           egg_counter_get (counter),
           counter->description);
}

static gboolean
int_parse_with_range (gint        *value,
                      gint         lower,
                      gint         upper,
                      const gchar *str)
{
  gint64 v64;

  g_assert (value);
  g_assert (lower <= upper);

  v64 = g_ascii_strtoll (str, NULL, 10);

  if (((v64 == G_MININT64) || (v64 == G_MAXINT64)) && (errno == ERANGE))
    return FALSE;

  if ((v64 < lower) || (v64 > upper))
    return FALSE;

  *value = (gint)v64;

  return TRUE;
}

gint
main (gint   argc,
      gchar *argv[])
{
  EggCounterArena *arena;
  guint n_counters = 0;
  gint pid;

  if (argc != 2)
    {
      fprintf (stderr, "usage: %s <pid>\n", argv [0]);
      return EXIT_FAILURE;
    }

  if (g_str_has_prefix (argv [1], "/dev/shm/EggCounters-"))
    argv [1] += strlen ("/dev/shm/EggCounters-");

  if (!int_parse_with_range (&pid, 1, G_MAXUSHORT, argv [1]))
    {
      fprintf (stderr, "usage: %s <pid>\n", argv [0]);
      return EXIT_FAILURE;
    }

  arena = egg_counter_arena_new_for_pid (pid);

  if (!arena)
    {
      fprintf (stderr, "Failed to access counters for process %u.\n", (int)pid);
      return EXIT_FAILURE;
    }

  g_print ("%-20s : %-32s : %20s : %-72s\n",
           "      Category",
           "             Name", "Value", "Description");
  g_print ("-------------------- : "
           "-------------------------------- : "
           "-------------------- : "
           "------------------------------------------------------------------------\n");
  egg_counter_arena_foreach (arena, foreach_cb, &n_counters);
  g_print ("-------------------- : "
           "-------------------------------- : "
           "-------------------- : "
           "------------------------------------------------------------------------\n");
  g_print ("Discovered %u counters\n", n_counters);

  return EXIT_SUCCESS;
}
