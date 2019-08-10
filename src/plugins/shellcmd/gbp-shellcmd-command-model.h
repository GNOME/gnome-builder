/* gbp-shellcmd-command-model.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "gbp-shellcmd-command.h"

G_BEGIN_DECLS

#define GBP_TYPE_SHELLCMD_COMMAND_MODEL (gbp_shellcmd_command_model_get_type())

G_DECLARE_FINAL_TYPE (GbpShellcmdCommandModel, gbp_shellcmd_command_model, GBP, SHELLCMD_COMMAND_MODEL, GObject)

GbpShellcmdCommandModel *gbp_shellcmd_command_model_new         (void);
GbpShellcmdCommand      *gbp_shellcmd_command_model_get_command (GbpShellcmdCommandModel  *self,
                                                                 const gchar              *command_id);
void                     gbp_shellcmd_command_model_add         (GbpShellcmdCommandModel  *self,
                                                                 GbpShellcmdCommand       *command);
void                     gbp_shellcmd_command_model_remove      (GbpShellcmdCommandModel  *self,
                                                                 GbpShellcmdCommand       *command);
void                     gbp_shellcmd_command_model_query       (GbpShellcmdCommandModel  *self,
                                                                 GPtrArray                *items,
                                                                 const gchar              *typed_text);
gboolean                 gbp_shellcmd_command_model_load        (GbpShellcmdCommandModel  *self,
                                                                 GCancellable             *cancellable,
                                                                 GError                  **error);
gboolean                 gbp_shellcmd_command_model_save        (GbpShellcmdCommandModel  *self,
                                                                 GCancellable             *cancellable,
                                                                 GError                  **error);

G_END_DECLS
