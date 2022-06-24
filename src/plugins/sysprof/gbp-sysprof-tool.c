/* gbp-sysprof-tool.c
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

#define G_LOG_DOMAIN "gbp-sysprof-tool"

#include "config.h"

#include "gbp-sysprof-tool.h"

struct _GbpSysprofTool
{
  IdeRunTool parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpSysprofTool, gbp_sysprof_tool, IDE_TYPE_RUN_TOOL)

static void
gbp_sysprof_tool_prepare_to_run (IdeRunTool    *run_tool,
                                 IdePipeline   *pipeline,
                                 IdeRunCommand *run_command,
                                 IdeRunContext *run_context)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_TOOL (run_tool));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_COMMAND (run_command));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  g_printerr ("TODO: Port sysprof tool\n");

  IDE_EXIT;
}

static void
gbp_sysprof_tool_send_signal (IdeRunTool *run_tool,
                              int         signum)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_TOOL (run_tool));

  IDE_EXIT;
}

static void
gbp_sysprof_tool_started (IdeRunTool    *run_tool,
                          IdeSubprocess *subprocess)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_TOOL (run_tool));
  g_assert (IDE_IS_SUBPROCESS (subprocess));

  IDE_EXIT;
}

static void
gbp_sysprof_tool_stopped (IdeRunTool *run_tool)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_TOOL (run_tool));

  IDE_EXIT;
}

static void
gbp_sysprof_tool_class_init (GbpSysprofToolClass *klass)
{
  IdeRunToolClass *run_tool_class = IDE_RUN_TOOL_CLASS (klass);

  run_tool_class->prepare_to_run = gbp_sysprof_tool_prepare_to_run;
  run_tool_class->send_signal = gbp_sysprof_tool_send_signal;
  run_tool_class->started = gbp_sysprof_tool_started;
  run_tool_class->stopped = gbp_sysprof_tool_stopped;
}

static void
gbp_sysprof_tool_init (GbpSysprofTool *self)
{
  ide_run_tool_set_icon_name (IDE_RUN_TOOL (self), "builder-profiler-symbolic");
}
