/* gbp-flatpak-client.h
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

#include <libide-core.h>

#include "daemon/ipc-flatpak-service.h"

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_CLIENT (gbp_flatpak_client_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakClient, gbp_flatpak_client, GBP, FLATPAK_CLIENT, GObject)

GbpFlatpakClient  *gbp_flatpak_client_get_default        (void);
IpcFlatpakService *gbp_flatpak_client_get_service        (GbpFlatpakClient     *self,
                                                          GCancellable         *cancellable,
                                                          GError              **error);
void               gbp_flatpak_client_get_service_async  (GbpFlatpakClient     *self,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
IpcFlatpakService *gbp_flatpak_client_get_service_finish (GbpFlatpakClient     *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);
void               gbp_flatpak_client_force_exit         (GbpFlatpakClient     *self);

G_END_DECLS
