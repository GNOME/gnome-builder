/* ipc-flatpak-service-impl.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include <flatpak/flatpak.h>

G_BEGIN_DECLS

#define IPC_TYPE_FLATPAK_REPO (ipc_flatpak_repo_get_type())

G_DECLARE_FINAL_TYPE (IpcFlatpakRepo, ipc_flatpak_repo, IPC, FLATPAK_REPO, GObject)

IpcFlatpakRepo      *ipc_flatpak_repo_get_default      (void);
void                 ipc_flatpak_repo_load             (const char      *data_dir);
FlatpakInstallation *ipc_flatpak_repo_get_installation (IpcFlatpakRepo  *self);
char                *ipc_flatpak_repo_get_path         (IpcFlatpakRepo  *self);
char                *ipc_flatpak_repo_get_config_dir   (IpcFlatpakRepo  *self);

G_END_DECLS
