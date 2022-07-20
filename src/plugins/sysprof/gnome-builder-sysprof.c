/* gnome-builder-sysprof.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <sysprof.h>
#include <unistd.h>

static int read_fd = -1;
static int write_fd = -1;
static char **env = NULL;
static gboolean aid_cpu;
static gboolean aid_perf;
static gboolean aid_memory;
static gboolean aid_memprof;
static gboolean aid_disk;
static gboolean aid_net;
static gboolean aid_energy;
static gboolean aid_battery;
static gboolean aid_compositor;
static gboolean aid_tracefd;
static gboolean no_throttle;
static const GOptionEntry options[] = {
  { "read-fd", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &read_fd },
  { "write-fd", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &write_fd },
  { "env", 0, 0, G_OPTION_ARG_STRING_ARRAY, &env, "Add an environment variable to the spawned process", "KEY=VALUE" },
  { "cpu", 0, 0, G_OPTION_ARG_NONE, &aid_cpu, "Track CPU usage and frequency" },
  { "perf", 0, 0, G_OPTION_ARG_NONE, &aid_perf, "Record stack traces with perf" },
  { "memory", 0, 0, G_OPTION_ARG_NONE, &aid_memory, "Record basic system memory usage" },
  { "memprof", 0, 0, G_OPTION_ARG_NONE, &aid_memprof, "Record stack traces during memory allocations" },
  { "disk", 0, 0, G_OPTION_ARG_NONE, &aid_disk, "Record disk usage information" },
  { "net", 0, 0, G_OPTION_ARG_NONE, &aid_net, "Record network usage information" },
  { "energy", 0, 0, G_OPTION_ARG_NONE, &aid_energy, "Record energy usage using RAPL" },
  { "battery", 0, 0, G_OPTION_ARG_NONE, &aid_battery, "Record battery charge and discharge rates" },
  { "compositor", 0, 0, G_OPTION_ARG_NONE, &aid_compositor, "Record GNOME Shell compositor information" },
  { "no-throttle", 0, 0, G_OPTION_ARG_NONE, &no_throttle, "Disable CPU throttling" },
  { "tracefd-aid", 0, 0, G_OPTION_ARG_NONE, &aid_tracefd, "Provide TRACEFD to subprocess" },
  { NULL }
};

static void
split_argv (int     argc,
            char  **argv,
            int    *our_argc,
            char ***our_argv,
            int    *sub_argc,
            char ***sub_argv)
{
  gboolean found_split = FALSE;

  *our_argc = 0;
  *our_argv = g_new0 (char *, 1);

  *sub_argc = 0;
  *sub_argv = g_new0 (char *, 1);

  for (int i = 0; i < argc; i++)
    {
      if (g_strcmp0 (argv[i], "--") == 0)
        {
          found_split = TRUE;
        }
      else if (found_split)
        {
          (*sub_argv) = g_realloc_n (*sub_argv, *sub_argc + 2, sizeof (char *));
          (*sub_argv)[*sub_argc] = g_strdup (argv[i]);
          (*sub_argv)[*sub_argc+1] = NULL;
          (*sub_argc)++;
        }
      else
        {
          (*our_argv) = g_realloc_n (*our_argv, *our_argc + 2, sizeof (char *));
          (*our_argv)[*our_argc] = g_strdup (argv[i]);
          (*our_argv)[*our_argc+1] = NULL;
          (*our_argc)++;
        }
    }
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) our_argv = NULL;
  g_auto(GStrv) sub_argv = NULL;
  int our_argc;
  int sub_argc;

  sysprof_clock_init ();

  split_argv (argc, argv, &our_argc, &our_argv, &sub_argc, &sub_argv);

  context = g_option_context_new ("-- COMMAND");
  g_option_context_add_main_entries (context, options, NULL);

  if (!g_option_context_parse (context, &our_argc, &our_argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }


  return EXIT_SUCCESS;
}
