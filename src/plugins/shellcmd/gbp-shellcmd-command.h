/* gbp-shellcmd-command.h
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

#include <libide-gui.h>
#include <libide-threading.h>

G_BEGIN_DECLS

#define GBP_TYPE_SHELLCMD_COMMAND (gbp_shellcmd_command_get_type())

typedef enum
{
  GBP_SHELLCMD_COMMAND_LOCALITY_HOST,
  GBP_SHELLCMD_COMMAND_LOCALITY_APP,
  GBP_SHELLCMD_COMMAND_LOCALITY_BUILD,
  GBP_SHELLCMD_COMMAND_LOCALITY_RUN,
} GbpShellcmdCommandLocality;

G_DECLARE_FINAL_TYPE (GbpShellcmdCommand, gbp_shellcmd_command, GBP, SHELLCMD_COMMAND, IdeObject)

GbpShellcmdCommand         *gbp_shellcmd_command_from_key_file     (GKeyFile                    *key_file,
                                                                    const gchar                 *group,
                                                                    GError                     **error);
void                        gbp_shellcmd_command_to_key_file       (GbpShellcmdCommand          *self,
                                                                    GKeyFile                    *key_file);
GbpShellcmdCommand         *gbp_shellcmd_command_copy              (GbpShellcmdCommand          *self);
const gchar                *gbp_shellcmd_command_get_id            (GbpShellcmdCommand          *self);
GbpShellcmdCommandLocality  gbp_shellcmd_command_get_locality      (GbpShellcmdCommand          *self);
void                        gbp_shellcmd_command_set_locality      (GbpShellcmdCommand          *self,
                                                                    GbpShellcmdCommandLocality   locality);
const gchar                *gbp_shellcmd_command_get_command       (GbpShellcmdCommand          *self);
void                        gbp_shellcmd_command_set_command       (GbpShellcmdCommand          *self,
                                                                    const gchar                 *command);
const gchar                *gbp_shellcmd_command_get_cwd           (GbpShellcmdCommand          *self);
void                        gbp_shellcmd_command_set_cwd           (GbpShellcmdCommand          *self,
                                                                    const gchar                 *cwd);
IdeEnvironment             *gbp_shellcmd_command_get_environment   (GbpShellcmdCommand          *self);
void                        gbp_shellcmd_command_set_priority      (GbpShellcmdCommand          *self,
                                                                    gint                         priority);
const gchar                *gbp_shellcmd_command_get_shortcut      (GbpShellcmdCommand          *self);
void                        gbp_shellcmd_command_set_shortcut      (GbpShellcmdCommand          *self,
                                                                    const gchar                 *shortcut);
void                        gbp_shellcmd_command_set_subtitle      (GbpShellcmdCommand          *self,
                                                                    const gchar                 *subtitle);
void                        gbp_shellcmd_command_set_title         (GbpShellcmdCommand          *self,
                                                                    const gchar                 *title);
gboolean                    gbp_shellcmd_command_get_close_on_exit (GbpShellcmdCommand          *self);
void                        gbp_shellcmd_command_set_close_on_exit (GbpShellcmdCommand          *self,
                                                                    gboolean                     close_on_exit);

G_END_DECLS
