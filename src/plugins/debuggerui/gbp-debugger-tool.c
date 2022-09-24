/* gbp-debugger-tool.c
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

#define G_LOG_DOMAIN "gbp-debugger-tool"

#include "config.h"

#include <libide-debugger.h>
#include <libide-gui.h>

#include "ide-debug-manager-private.h"

#include "gbp-debugger-tool.h"
#include "ide-debugger-workspace-addin.h"

struct _GbpDebuggerTool
{
  IdeRunTool parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpDebuggerTool, gbp_debugger_tool, IDE_TYPE_RUN_TOOL)

static void
gbp_debugger_tool_send_signal (IdeRunTool *run_tool,
                               int         signum)
{
  GbpDebuggerTool *self = (GbpDebuggerTool *)run_tool;
  IdeDebugManager *debug_manager;
  IdeDebugger *debugger;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DEBUGGER_TOOL (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  debug_manager = ide_debug_manager_from_context (context);
  debugger = ide_debug_manager_get_debugger (debug_manager);

  if (debugger != NULL)
    ide_debugger_send_signal_async (debugger, signum, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
gbp_debugger_tool_prepare_to_run (IdeRunTool    *run_tool,
                                  IdePipeline   *pipeline,
                                  IdeRunCommand *run_command,
                                  IdeRunContext *run_context)
{
  GbpDebuggerTool *self = (GbpDebuggerTool *)run_tool;
  IdeDebugManager *debug_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DEBUGGER_TOOL (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_COMMAND (run_command));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  context = ide_object_get_context (IDE_OBJECT (self));
  debug_manager = ide_debug_manager_from_context (context);

  _ide_debug_manager_prepare (debug_manager, pipeline, run_command, run_context, NULL);

  IDE_EXIT;
}

static void
gbp_debugger_tool_started (IdeRunTool    *run_tool,
                           IdeSubprocess *subprocess)
{
  GbpDebuggerTool *self = (GbpDebuggerTool *)run_tool;
  IdeWorkspaceAddin *addin;
  IdeDebugManager *debug_manager;
  IdeWorkbench *workbench;
  IdeWorkspace *workspace;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DEBUGGER_TOOL (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));

  context = ide_object_get_context (IDE_OBJECT (self));

  /* Make sure controls are visible to user */
  workbench = ide_workbench_from_context (context);
  workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_PRIMARY_WORKSPACE);
  addin = ide_workspace_addin_find_by_module_name (workspace, "debuggerui");
  ide_debugger_workspace_addin_raise_panel (IDE_DEBUGGER_WORKSPACE_ADDIN (addin));

  /* Notify debug manager we've started so it can sync breakpoints */
  debug_manager = ide_debug_manager_from_context (context);
  _ide_debug_manager_started (debug_manager);

  IDE_EXIT;
}

static void
gbp_debugger_tool_stopped (IdeRunTool *run_tool)
{
  GbpDebuggerTool *self = (GbpDebuggerTool *)run_tool;
  IdeDebugManager *debug_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DEBUGGER_TOOL (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  debug_manager = ide_debug_manager_from_context (context);

  _ide_debug_manager_stopped (debug_manager);

  IDE_EXIT;
}

static void
gbp_debugger_tool_class_init (GbpDebuggerToolClass *klass)
{
  IdeRunToolClass *run_tool_class = IDE_RUN_TOOL_CLASS (klass);

  run_tool_class->prepare_to_run = gbp_debugger_tool_prepare_to_run;
  run_tool_class->send_signal = gbp_debugger_tool_send_signal;
  run_tool_class->started = gbp_debugger_tool_started;
  run_tool_class->stopped = gbp_debugger_tool_stopped;
}

static void
gbp_debugger_tool_init (GbpDebuggerTool *self)
{
  ide_run_tool_set_icon_name (IDE_RUN_TOOL (self),
                              "builder-debugger-symbolic");
}
