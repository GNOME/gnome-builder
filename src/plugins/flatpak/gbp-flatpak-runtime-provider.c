/* gbp-flatpak-runtime-provider.h
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

#define G_LOG_DOMAIN "gbp-flatpak-runtime-provider"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-flatpak-client.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-runtime-provider.h"

#include "daemon/ipc-flatpak-service.h"
#include "daemon/ipc-flatpak-util.h"

struct _GbpFlatpakRuntimeProvider
{
  IdeObject parent_instance;
  GPtrArray *runtimes;
};

static void runtime_provider_iface_init (IdeRuntimeProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakRuntimeProvider, gbp_flatpak_runtime_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER, runtime_provider_iface_init))

static void
gbp_flatpak_runtime_provider_dispose (GObject *object)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)object;

  g_clear_pointer (&self->runtimes, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_flatpak_runtime_provider_parent_class)->dispose (object);
}

static void
gbp_flatpak_runtime_provider_class_init (GbpFlatpakRuntimeProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_flatpak_runtime_provider_dispose;
}

static void
gbp_flatpak_runtime_provider_init (GbpFlatpakRuntimeProvider *self)
{
  self->runtimes = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
on_runtime_added_cb (GbpFlatpakRuntimeProvider *self,
                     GVariant                  *info,
                     IpcFlatpakService         *service)

{
  g_autoptr(GbpFlatpakRuntime) runtime = NULL;
  g_autoptr(IdeContext) context = NULL;
  IdeRuntimeManager *manager;
  const gchar *name;
  const gchar *arch;
  const gchar *branch;
  const gchar *sdk_name;
  const gchar *sdk_branch;
  const gchar *deploy_dir;
  const gchar *metadata;
  gboolean is_extension;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (info != NULL);
  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (g_variant_is_of_type (info, RUNTIME_VARIANT_TYPE));

  if (self->runtimes == NULL)
    return;

  if (!runtime_variant_parse (info,
                              &name, &arch, &branch,
                              &sdk_name, &sdk_branch,
                              &deploy_dir,
                              &metadata,
                              &is_extension))
    return;

  /* Ignore extensions for now */
  if (is_extension)
    return;

  context = ide_object_ref_context (IDE_OBJECT (self));
  manager = ide_runtime_manager_from_context (context);
  runtime = gbp_flatpak_runtime_new (name,
                                     arch,
                                     branch,
                                     sdk_name,
                                     sdk_branch,
                                     deploy_dir,
                                     metadata,
                                     is_extension);
  g_ptr_array_add (self->runtimes, g_object_ref (runtime));
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (runtime));
  ide_runtime_manager_add (manager, IDE_RUNTIME (runtime));
}

static void
gbp_flatpak_runtime_provider_load_list_runtimes_cb (GObject      *object,
                                                    GAsyncResult *result,
                                                    gpointer      user_data)
{
  IpcFlatpakService *service = (IpcFlatpakService *)object;
  g_autoptr(GbpFlatpakRuntimeProvider) self = user_data;
  g_autoptr(GVariant) runtimes = NULL;
  g_autoptr(GError) error = NULL;
  GVariantIter iter;
  GVariant *info;

  g_assert (IPC_IS_FLATPAK_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));

  if (!ipc_flatpak_service_call_list_runtimes_finish (service, &runtimes, result, &error))
    {
      g_warning ("Failed to list flatpak runtimes: %s", error->message);
      return;
    }

  g_variant_iter_init (&iter, runtimes);
  while ((info = g_variant_iter_next_value (&iter)))
    {
      on_runtime_added_cb (self, info, service);
      g_variant_unref (info);
    }
}

static void
gbp_flatpak_runtime_provider_load (IdeRuntimeProvider *provider,
                                   IdeRuntimeManager  *manager)
{
  g_autoptr(GbpFlatpakClient) client = NULL;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(IdeContext) context = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (provider));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  if ((context = ide_object_ref_context (IDE_OBJECT (provider))) &&
      (client = gbp_flatpak_client_ensure (context)) &&
      (service = gbp_flatpak_client_get_service (client, NULL, NULL)))
    {
      g_signal_connect_object (service,
                               "runtime-added",
                               G_CALLBACK (on_runtime_added_cb),
                               provider,
                               G_CONNECT_SWAPPED);
      ipc_flatpak_service_call_list_runtimes (service,
                                              NULL,
                                              gbp_flatpak_runtime_provider_load_list_runtimes_cb,
                                              g_object_ref (provider));
    }
}

static void
gbp_flatpak_runtime_provider_unload (IdeRuntimeProvider *provider,
                                     IdeRuntimeManager  *manager)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;

  g_assert (IDE_IS_RUNTIME_PROVIDER (provider));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  if (self->runtimes == NULL || self->runtimes->len == 0)
    return;

  for (guint i = 0; i < self->runtimes->len; i++)
    ide_runtime_manager_remove (manager, g_ptr_array_index (self->runtimes, i));
  g_ptr_array_remove_range (self->runtimes, 0, self->runtimes->len);
}

static void
runtime_provider_iface_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_flatpak_runtime_provider_load;
  iface->unload = gbp_flatpak_runtime_provider_unload;
}
