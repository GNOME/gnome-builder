/* gbp-host-runtime-provider.c
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

#define G_LOG_DOMAIN "gbp-host-runtime-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-host-runtime.h"
#include "gbp-host-runtime-provider.h"
#include "gbp-noop-runtime.h"

struct _GbpHostRuntimeProvider
{
  IdeRuntimeProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpHostRuntimeProvider, gbp_host_runtime_provider, IDE_TYPE_RUNTIME_PROVIDER)

static DexFuture *
gbp_host_runtime_provider_load (IdeRuntimeProvider *provider)
{
  GbpHostRuntimeProvider *self = (GbpHostRuntimeProvider *)provider;
  g_autoptr(IdeRuntime) host = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_HOST_RUNTIME_PROVIDER (self));

  host = g_object_new (GBP_TYPE_HOST_RUNTIME,
                       "id", "host",
                       "name", _("Host Operating System"),
                       "category", _("Host System"),
                       NULL);
  ide_runtime_provider_add (IDE_RUNTIME_PROVIDER (self), IDE_RUNTIME (host));

  if (ide_is_flatpak ())
    {
      g_autoptr(IdeRuntime) noop = NULL;

      /* Allow using Builder itself as a runtime/SDK to allow for
       * cases where there are no other toolchain options.
       */
      noop = g_object_new (GBP_TYPE_NOOP_RUNTIME,
                           "id", "noop",
                           /* translators: Bundled means a runtime "bundled" with Builder */
                           "name", _("Bundled with Builder"),
                           "category", _("Host System"),
                           NULL);
      ide_runtime_provider_add (IDE_RUNTIME_PROVIDER (self), IDE_RUNTIME (noop));
    }

  IDE_RETURN (dex_future_new_for_boolean (TRUE));
}

static void
gbp_host_runtime_provider_class_init (GbpHostRuntimeProviderClass *klass)
{
  IdeRuntimeProviderClass *runtime_provider_class = IDE_RUNTIME_PROVIDER_CLASS (klass);

  runtime_provider_class->load = gbp_host_runtime_provider_load;
}

static void
gbp_host_runtime_provider_init (GbpHostRuntimeProvider *self)
{
}
