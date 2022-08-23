/* ipc-flatpak-transfer-impl.h
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

#include <gtk/gtk.h>

#include <libide-core.h>

#include "daemon/ipc-flatpak-transfer.h"

G_BEGIN_DECLS

#define IPC_TYPE_FLATPAK_TRANSFER_IMPL (ipc_flatpak_transfer_impl_get_type())

G_DECLARE_FINAL_TYPE (IpcFlatpakTransferImpl, ipc_flatpak_transfer_impl, IPC, FLATPAK_TRANSFER_IMPL, IpcFlatpakTransferSkeleton)

IpcFlatpakTransfer *ipc_flatpak_transfer_impl_new        (IdeContext *context);
IpcFlatpakTransfer *ipc_flatpak_transfer_impl_new_simple (GtkWindow  *toplevel);

G_END_DECLS
