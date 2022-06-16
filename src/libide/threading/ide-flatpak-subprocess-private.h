/* ide-flatpak-subprocess-private.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-subprocess.h"
#include "ide-unix-fd-map.h"

G_BEGIN_DECLS

#define IDE_TYPE_FLATPAK_SUBPROCESS (ide_flatpak_subprocess_get_type())

G_DECLARE_FINAL_TYPE (IdeFlatpakSubprocess, ide_flatpak_subprocess, IDE, FLATPAK_SUBPROCESS, GObject)

IdeSubprocess *_ide_flatpak_subprocess_new (const char          *cwd,
                                            const char * const  *argv,
                                            const char * const  *env,
                                            GSubprocessFlags     flags,
                                            gboolean             clear_flags,
                                            IdeUnixFDMap        *unix_fd_map,
                                            GCancellable        *cancellable,
                                            GError             **error);

G_END_DECLS
