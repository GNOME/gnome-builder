/* ide-foundry-global.h
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

G_BEGIN_DECLS

IDE_AVAILABLE_IN_ALL
int                    ide_foundry_bytes_to_memfd           (GBytes      *bytes,
                                                             const char  *name);
IDE_AVAILABLE_IN_ALL
int                    ide_foundry_file_to_memfd            (GFile       *file,
                                                             const char  *name);
IDE_AVAILABLE_IN_ALL
IdeSubprocessLauncher *ide_foundry_get_launcher_for_context (IdeContext  *context,
                                                             const char  *program_name,
                                                             const char  *bundled_program_path,
                                                             GError     **error);


G_END_DECLS
