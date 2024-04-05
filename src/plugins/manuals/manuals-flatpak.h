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

#include "manuals-flatpak-runtime.h"

#include "../flatpak/gbp-flatpak-client.h"
#include "../flatpak/daemon/ipc-flatpak-util.h"

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
                                        dex_ref (promise));
  return DEX_FUTURE (promise);
}

static inline void
manuals_flatpak_service_list_runtimes_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IpcFlatpakService *service = IPC_FLATPAK_SERVICE (object);
  g_autoptr(DexPromise) promise = user_data;
  GError *error = NULL;
  GVariant *reply = NULL;

  if (!ipc_flatpak_service_call_list_runtimes_finish (service, &reply, result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_variant (promise, g_steal_pointer (&reply));

}

static inline DexFuture *
manuals_flatpak_service_list_runtimes (IpcFlatpakService *service)
{
  DexPromise *promise = dex_promise_new_cancellable ();
  ipc_flatpak_service_call_list_runtimes (service,
                                          dex_promise_get_cancellable (promise),
                                          manuals_flatpak_service_list_runtimes_cb,
                                          dex_ref (promise));
  return DEX_FUTURE (promise);
}

static inline DexFuture *
manuals_flatpak_list_runtimes_fiber (gpointer user_data)
{
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  GVariantIter iter;
  GVariant *info;

  g_assert (IDE_IS_MAIN_THREAD ());

  if (!(service = dex_await_object (manuals_flatpak_get_service (), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(reply = dex_await_variant (manuals_flatpak_service_list_runtimes (service), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  store = g_list_store_new (MANUALS_TYPE_FLATPAK_RUNTIME);

  g_variant_iter_init (&iter, reply);

  while ((info = g_variant_iter_next_value (&iter)))
    {
      const char *name;
      const char *arch;
      const char *branch;
      const char *sdk_name;
      const char *sdk_branch;
      const char *deploy_dir;
      const char *metadata;
      gboolean is_extension;

      if (runtime_variant_parse (info,
                                 &name, &arch, &branch,
                                 &sdk_name, &sdk_branch,
                                 &deploy_dir,
                                 &metadata,
                                 &is_extension))
        {
          g_autoptr(ManualsFlatpakRuntime) runtime = NULL;

          runtime = g_object_new (MANUALS_TYPE_FLATPAK_RUNTIME,
                                  "name", name,
                                  "arch", arch,
                                  "branch", branch,
                                  "deploy-dir", deploy_dir,
                                  NULL);

          g_list_store_append (store, runtime);
        }

      g_variant_unref (info);
    }

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static inline DexFuture *
manuals_flatpak_list_runtimes (void)
{
  return dex_scheduler_spawn (NULL, 0,
                              manuals_flatpak_list_runtimes_fiber,
                              NULL, NULL);
}

G_END_DECLS
