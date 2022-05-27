/* ide-diagnostic-tool.h
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

#include <libide-code.h>
#include <libide-core.h>
#include <libide-threading.h>

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTIC_TOOL (ide_diagnostic_tool_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDiagnosticTool, ide_diagnostic_tool, IDE, DIAGNOSTIC_TOOL, IdeObject)

struct _IdeDiagnosticToolClass
{
  IdeObjectClass parent_class;

  IdeSubprocessLauncher *(*create_launcher)      (IdeDiagnosticTool      *self,
                                                  const char             *program_name,
                                                  GFile                  *file,
                                                  GBytes                 *contents,
                                                  const char             *language_id,
                                                  GError                **error);
  void                   (*configure_launcher)   (IdeDiagnosticTool      *self,
                                                  IdeSubprocessLauncher  *launcher,
                                                  GFile                  *file,
                                                  GBytes                 *contents,
                                                  const char             *language_id);
  GBytes                *(*get_stdin_bytes)      (IdeDiagnosticTool      *self,
                                                  GFile                  *file,
                                                  GBytes                 *contents,
                                                  const char             *language_id);
  void                   (*populate_diagnostics) (IdeDiagnosticTool      *self,
                                                  IdeDiagnostics         *diagnostics,
                                                  GFile                  *file,
                                                  const char             *stdout_buf,
                                                  const char             *stderr_buf);
  gboolean               (*can_diagnose)         (IdeDiagnosticTool      *self,
                                                  GFile                  *file,
                                                  GBytes                 *contents,
                                                  const char             *language_id);
};

IDE_AVAILABLE_IN_ALL
const char *ide_diagnostic_tool_get_program_name         (IdeDiagnosticTool *self);
IDE_AVAILABLE_IN_ALL
void        ide_diagnostic_tool_set_program_name         (IdeDiagnosticTool *self,
                                                          const char        *program_name);
IDE_AVAILABLE_IN_ALL
const char *ide_diagnostic_tool_get_bundled_program_path (IdeDiagnosticTool *self);
IDE_AVAILABLE_IN_ALL
void        ide_diagnostic_tool_set_bundled_program_path (IdeDiagnosticTool *self,
                                                          const char        *path);
IDE_AVAILABLE_IN_ALL
const char *ide_diagnostic_tool_get_local_program_path   (IdeDiagnosticTool *self);
IDE_AVAILABLE_IN_ALL
void        ide_diagnostic_tool_set_local_program_path   (IdeDiagnosticTool *self,
                                                          const char        *path);

G_END_DECLS
