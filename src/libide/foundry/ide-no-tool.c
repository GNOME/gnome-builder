/* ide-no-tool.c
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

#define G_LOG_DOMAIN "ide-no-tool"

#include "config.h"

#include "ide-no-tool-private.h"
#include "ide-pipeline.h"
#include "ide-run-command.h"
#include "ide-run-context.h"

struct _IdeNoTool
{
  IdeRunTool parent_instance;
};

G_DEFINE_FINAL_TYPE (IdeNoTool, ide_no_tool, IDE_TYPE_RUN_TOOL)

static void
ide_no_tool_prepare_to_run (IdeRunTool    *run_tool,
                            IdePipeline   *pipeline,
                            IdeRunCommand *run_command,
                            IdeRunContext *run_context)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_COMMAND (run_command));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  IDE_EXIT;
}

static void
ide_no_tool_class_init (IdeNoToolClass *klass)
{
  IdeRunToolClass *run_tool_class = IDE_RUN_TOOL_CLASS (klass);

  run_tool_class->prepare_to_run = ide_no_tool_prepare_to_run;
}

static void
ide_no_tool_init (IdeNoTool *self)
{
  ide_run_tool_set_icon_name (IDE_RUN_TOOL (self),
                              "builder-run-start-symbolic");
}

IdeRunTool *
ide_no_tool_new (void)
{
  return g_object_new (IDE_TYPE_NO_TOOL, NULL);
}
