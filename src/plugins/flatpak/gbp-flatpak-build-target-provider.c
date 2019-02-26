/* gbp-flatpak-build-target-provider.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-build-target-provider"

#include "gbp-flatpak-build-target.h"
#include "gbp-flatpak-build-target-provider.h"
#include "gbp-flatpak-manifest.h"

struct _GbpFlatpakBuildTargetProvider
{
  IdeObject parent_instance;
};

static void
gbp_flatpak_build_target_provider_get_targets_async (IdeBuildTargetProvider *provider,
                                                     GCancellable           *cancellable,
                                                     GAsyncReadyCallback     callback,
                                                     gpointer                user_data)
{
  GbpFlatpakBuildTargetProvider *self = (GbpFlatpakBuildTargetProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) targets = NULL;
  IdeConfigManager *config_manager;
  IdeConfig *config;
  IdeContext *context;

  g_assert (GBP_IS_FLATPAK_BUILD_TARGET_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_build_target_provider_get_targets_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);

  targets = g_ptr_array_new_with_free_func (g_object_unref);

  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      g_autoptr(IdeBuildTarget) target = NULL;
      const gchar *command;

      command = gbp_flatpak_manifest_get_command (GBP_FLATPAK_MANIFEST (config));

      target = g_object_new (GBP_TYPE_FLATPAK_BUILD_TARGET,
                             "command", command,
                             NULL);

      g_ptr_array_add (targets, g_steal_pointer (&target));
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&targets),
                           g_ptr_array_unref);
}

static GPtrArray *
gbp_flatpak_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *provider,
                                                      GAsyncResult            *result,
                                                      GError                 **error)
{
  GPtrArray *ret;

  g_assert (GBP_IS_FLATPAK_BUILD_TARGET_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
build_target_provider_iface_init (IdeBuildTargetProviderInterface *iface)
{
  iface->get_targets_async = gbp_flatpak_build_target_provider_get_targets_async;
  iface->get_targets_finish = gbp_flatpak_build_target_provider_get_targets_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpFlatpakBuildTargetProvider,
                         gbp_flatpak_build_target_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET_PROVIDER,
                                                build_target_provider_iface_init))

static void
gbp_flatpak_build_target_provider_class_init (GbpFlatpakBuildTargetProviderClass *klass)
{
}

static void
gbp_flatpak_build_target_provider_init (GbpFlatpakBuildTargetProvider *self)
{
}
