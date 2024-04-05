/*
 * manuals-flatpak.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <libdex.h>

#include "../flatpak/gbp-flatpak-client.h"

G_BEGIN_DECLS

static inline void
manuals_flatpak_get_service_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GError *error = NULL;
  g_autoptr(DexPromise) promise = user_data;
  IpcFlatpakService *service = gbp_flatpak_client_get_service_finish (GBP_FLATPAK_CLIENT (object), result, &error);

  if (error != NULL)
    dex_promise_reject (promise, error);
  else
    dex_promise_resolve_object (promise, service);
}

static inline DexFuture *
manuals_flatpak_get_service (void)
{
  GbpFlatpakClient *client = gbp_flatpak_client_get_default ();
  DexPromise *promise = dex_promise_new_cancellable ();
  gbp_flatpak_client_get_service_async (client,
                                        dex_promise_get_cancellable (promise),
                                        manuals_flatpak_get_service_cb,
                                        dex_ref (promise),
                                        dex_unref);
  return DEX_FUTURE (promise);
}

G_END_DECLS
