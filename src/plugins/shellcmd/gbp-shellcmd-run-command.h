/* gbp-shellcmd-run-command.h
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

#include <libide-foundry.h>
#include <libide-terminal.h>

G_BEGIN_DECLS

/**
 * GbpShellcmdLocality:
 * %GBP_SHELLCMD_LOCALITY_SUBPROCESS: run as a subprocess of builder
 * %GBP_SHELLCMD_LOCALITY_HOST: run on the host system, possibly bypassing container
 * %GBP_SHELLCMD_LOCALITY_PIPELINE: run from build pipeline
 * %GBP_SHELLCMD_LOCALITY_RUNTIME: run like a target application
 */
typedef enum
{
  GBP_SHELLCMD_LOCALITY_SUBPROCESS = 0,
  GBP_SHELLCMD_LOCALITY_HOST,
  GBP_SHELLCMD_LOCALITY_PIPELINE,
  GBP_SHELLCMD_LOCALITY_RUNTIME,
} GbpShellcmdLocality;

#define GBP_TYPE_SHELLCMD_RUN_COMMAND (gbp_shellcmd_run_command_get_type())

G_DECLARE_FINAL_TYPE (GbpShellcmdRunCommand, gbp_shellcmd_run_command, GBP, SHELLCMD_RUN_COMMAND, IdeRunCommand)

GbpShellcmdRunCommand *gbp_shellcmd_run_command_new             (const char            *settings_path);
GbpShellcmdRunCommand *gbp_shellcmd_run_command_create          (IdeContext            *context);
void                   gbp_shellcmd_run_command_delete          (GbpShellcmdRunCommand *self);
const char            *gbp_shellcmd_run_command_get_accelerator (GbpShellcmdRunCommand *self);
void                   gbp_shellcmd_run_command_set_accelerator (GbpShellcmdRunCommand *self,
                                                                 const char            *accelerator);
GbpShellcmdLocality    gbp_shellcmd_run_command_get_locality    (GbpShellcmdRunCommand *self);
void                   gbp_shellcmd_run_command_set_locality    (GbpShellcmdRunCommand *self,
                                                                 GbpShellcmdLocality    locality);
const char            *gbp_shellcmd_run_command_get_keywords    (GbpShellcmdRunCommand *self);
char                  *gbp_shellcmd_run_command_dup_subtitle    (GbpShellcmdRunCommand *self);
gboolean               gbp_shellcmd_run_command_get_use_shell   (GbpShellcmdRunCommand *self);
void                   gbp_shellcmd_run_command_set_use_shell   (GbpShellcmdRunCommand *self,
                                                                 gboolean               use_shell);

G_END_DECLS
