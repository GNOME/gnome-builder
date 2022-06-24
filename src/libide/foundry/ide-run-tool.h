/* ide-run-tool.h
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

#pragma once

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>
#include <libide-threading.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUN_TOOL (ide_run_tool_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeRunTool, ide_run_tool, IDE, RUN_TOOL, IdeObject)

struct _IdeRunToolClass
{
  IdeObjectClass parent_class;

  void (*started)        (IdeRunTool    *self,
                          IdeSubprocess *subprocess);
  void (*stopped)        (IdeRunTool    *self);
  void (*prepare_to_run) (IdeRunTool    *self,
                          IdePipeline   *pipeline,
                          IdeRunCommand *run_command,
                          IdeRunContext *run_context);
  void (*force_exit)     (IdeRunTool    *self);
  void (*send_signal)    (IdeRunTool    *self,
                          int            signum);
};

IDE_AVAILABLE_IN_ALL
void        ide_run_tool_force_exit     (IdeRunTool    *self);
IDE_AVAILABLE_IN_ALL
void        ide_run_tool_send_signal    (IdeRunTool    *self,
                                         int            signum);
IDE_AVAILABLE_IN_ALL
void        ide_run_tool_prepare_to_run (IdeRunTool    *self,
                                         IdePipeline   *pipeline,
                                         IdeRunCommand *run_command,
                                         IdeRunContext *run_context);
IDE_AVAILABLE_IN_ALL
const char *ide_run_tool_get_icon_name  (IdeRunTool    *self);
IDE_AVAILABLE_IN_ALL
void        ide_run_tool_set_icon_name  (IdeRunTool    *self,
                                         const char    *icon_name);

G_END_DECLS
