/* gbp-phpize-build-system-discovery.c
 *
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-phpize-build-system-discovery"

#include "config.h"

#include "gbp-phpize-build-system-discovery.h"

struct _GbpPhpizeBuildSystemDiscovery
{
  IdeSimpleBuildSystemDiscovery parent_instance;
};

static char *
gbp_phpize_build_system_discovery_discover (IdeBuildSystemDiscovery  *discovery,
                                            GFile                    *directory,
                                            GCancellable             *cancellable,
                                            int                      *priority,
                                            GError                  **error)
{
  g_autoptr(GFile) config_m4 = NULL;
  g_autofree char *contents = NULL;
  char *ret = NULL;
  gsize len;

  IDE_ENTRY;

  g_assert (GBP_IS_PHPIZE_BUILD_SYSTEM_DISCOVERY (discovery));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (priority != NULL);

  config_m4 = g_file_get_child (directory, "config.m4");

  if (g_file_load_contents (config_m4, cancellable, &contents, &len, NULL, NULL))
    {
      if (strstr (contents, "PHP_ARG_ENABLE") != NULL)
        {
          g_debug ("Found PHP_ARG_ENABLE in configure.ac");
          ret = g_strdup ("phpize");
          *priority = 1000;
        }
    }

  IDE_RETURN (ret);
}

static void
build_system_discovery_iface_init (IdeBuildSystemDiscoveryInterface *iface)
{
  iface->discover = gbp_phpize_build_system_discovery_discover;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpPhpizeBuildSystemDiscovery, gbp_phpize_build_system_discovery, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM_DISCOVERY, build_system_discovery_iface_init))

static void
gbp_phpize_build_system_discovery_class_init (GbpPhpizeBuildSystemDiscoveryClass *klass)
{
}

static void
gbp_phpize_build_system_discovery_init (GbpPhpizeBuildSystemDiscovery *self)
{
}
