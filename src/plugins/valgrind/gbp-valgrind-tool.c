/* gbp-valgrind-tool.c
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

#define G_LOG_DOMAIN "gbp-valgrind-tool"

#include "config.h"

#include <libide-gui.h>

#include "gbp-valgrind-tool.h"

struct _GbpValgrindTool
{
  IdeRunTool  parent_instance;
  char       *log_name;
};

G_DEFINE_FINAL_TYPE (GbpValgrindTool, gbp_valgrind_tool, IDE_TYPE_RUN_TOOL)

static gboolean
gbp_valgrind_tool_handler_cb (IdeRunContext       *run_context,
                              const char * const  *argv,
                              const char * const  *env,
                              const char          *cwd,
                              IdeUnixFDMap        *unix_fd_map,
                              gpointer             user_data,
                              GError             **error)
{
  GbpValgrindTool *self = user_data;
  g_autoptr(IdeSettings) settings = NULL;
  g_autoptr(GString) leak_kinds = NULL;
  g_autofree char *name = NULL;
  g_autofree char *leak_check = NULL;
  IdeContext *context;
  gboolean track_origins;
  guint num_callers;
  int source_fd;
  int dest_fd;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (GBP_IS_VALGRIND_TOOL (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  settings = ide_context_ref_settings (context, "org.gnome.builder.valgrind");

  if (cwd != NULL)
    ide_run_context_set_cwd (run_context, cwd);

  dest_fd = ide_unix_fd_map_get_max_dest_fd (unix_fd_map) + 1;
  if (!ide_run_context_merge_unix_fd_map (run_context, unix_fd_map, error))
    IDE_RETURN (FALSE);

  /* Create a temp file to write to and an FD to access it */
  if (-1 == (source_fd = g_file_open_tmp ("gnome-builder-valgrind-XXXXXX.txt", &name, error)))
    IDE_RETURN (FALSE);

  /* Set our FD for valgrind to log to */
  ide_run_context_take_fd (run_context, source_fd, dest_fd);

  /* Save the filename so we can open it after exiting */
  g_set_str (&self->log_name, name);
  g_debug ("Using %s for valgrind log", name);

  track_origins = ide_settings_get_boolean (settings, "track-origins");
  num_callers = ide_settings_get_uint (settings, "num-callers");
  leak_check = ide_settings_get_string (settings, "leak-check");

  leak_kinds = g_string_new (NULL);
  if (ide_settings_get_boolean (settings, "leak-kind-definite"))
    g_string_append (leak_kinds, "definite,");
  if (ide_settings_get_boolean (settings, "leak-kind-possible"))
    g_string_append (leak_kinds, "possible,");
  if (ide_settings_get_boolean (settings, "leak-kind-indirect"))
    g_string_append (leak_kinds, "indirect,");
  if (ide_settings_get_boolean (settings, "leak-kind-reachable"))
    g_string_append (leak_kinds, "reachable,");

  ide_run_context_append_argv (run_context, "valgrind");
  ide_run_context_append_formatted (run_context, "--log-fd=%d", dest_fd);
  ide_run_context_append_formatted (run_context, "--leak-check=%s", leak_check);
  ide_run_context_append_formatted (run_context, "--track-origins=%s", track_origins ? "yes" : "no");
  ide_run_context_append_formatted (run_context, "--num-callers=%u", num_callers);

  if (leak_kinds->len > 0)
    {
      g_string_truncate (leak_kinds, leak_kinds->len-1);
      ide_run_context_append_formatted (run_context, "--show-leak-kinds=%s", leak_kinds->str);
    }

  if (env[0] != NULL)
    {
      /* If we have to exec "env" to pass environment variables, then we
       * must follow children to get to our target executable.
       */
      ide_run_context_append_argv (run_context, "--trace-children=yes");
      ide_run_context_append_argv (run_context, "env");
      ide_run_context_append_args (run_context, env);
    }

  ide_run_context_append_args (run_context, argv);

  IDE_RETURN (TRUE);
}

static void
gbp_valgrind_tool_prepare_to_run (IdeRunTool    *run_tool,
                                  IdePipeline   *pipeline,
                                  IdeRunCommand *run_command,
                                  IdeRunContext *run_context)
{
  GbpValgrindTool *self = (GbpValgrindTool *)run_tool;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VALGRIND_TOOL (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_COMMAND (run_command));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  ide_run_context_push (run_context,
                        gbp_valgrind_tool_handler_cb,
                        g_object_ref (self),
                        g_object_unref);

  IDE_EXIT;
}

static void
gbp_valgrind_tool_stopped (IdeRunTool *run_tool)
{
  GbpValgrindTool *self = (GbpValgrindTool *)run_tool;
  g_autoptr(GFile) file = NULL;
  IdeWorkbench *workbench;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VALGRIND_TOOL (self));

  if (self->log_name == NULL)
    IDE_EXIT;

  context = ide_object_get_context (IDE_OBJECT (self));
  workbench = ide_workbench_from_context (context);
  file = g_file_new_for_path (self->log_name);

  ide_workbench_open_async (workbench,
                            file,
                            "editorui",
                            IDE_BUFFER_OPEN_FLAGS_NONE,
                            NULL, NULL, NULL, NULL);

  g_clear_pointer (&self->log_name, g_free);

  IDE_EXIT;
}

static void
gbp_valgrind_tool_finalize (GObject *object)
{
  GbpValgrindTool *self = (GbpValgrindTool *)object;

  g_clear_pointer (&self->log_name, g_free);

  G_OBJECT_CLASS (gbp_valgrind_tool_parent_class)->finalize (object);
}

static void
gbp_valgrind_tool_class_init (GbpValgrindToolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRunToolClass *run_tool_class = IDE_RUN_TOOL_CLASS (klass);

  object_class->finalize = gbp_valgrind_tool_finalize;

  run_tool_class->prepare_to_run = gbp_valgrind_tool_prepare_to_run;
  run_tool_class->stopped = gbp_valgrind_tool_stopped;
}

static void
gbp_valgrind_tool_init (GbpValgrindTool *self)
{
  ide_run_tool_set_icon_name (IDE_RUN_TOOL (self), "builder-valgrind-symbolic");
}
