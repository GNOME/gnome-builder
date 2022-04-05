/* ide-buildconfig-build-target-provider.c
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

#define G_LOG_DOMAIN "ide-buildconfig-build-target-provider"

#include "config.h"

#include <libide-threading.h>

#include "ide-buildconfig-config.h"
#include "ide-buildconfig-build-target.h"
#include "ide-buildconfig-build-target-provider.h"

struct _IdeBuildconfigBuildTargetProvider
{
  IdeObject parent_instance;
};

static void
ide_buildconfig_build_target_provider_get_targets_async (IdeBuildTargetProvider *provider,
                                                         GCancellable           *cancellable,
                                                         GAsyncReadyCallback     callback,
                                                         gpointer                user_data)
{
  IdeBuildconfigBuildTargetProvider *self = (IdeBuildconfigBuildTargetProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) targets = NULL;
  IdeConfigManager *config_manager;
  IdeConfig *config;
  IdeContext *context;

  g_assert (IDE_IS_BUILDCONFIG_BUILD_TARGET_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buildconfig_build_target_provider_get_targets_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);

  targets = g_ptr_array_new_with_free_func (g_object_unref);

  if (IDE_IS_BUILDCONFIG_CONFIG (config))
    {
      const char * const *run_command;

      run_command = ide_buildconfig_config_get_run_command (IDE_BUILDCONFIG_CONFIG (config));

      if (run_command != NULL && run_command[0] != NULL)
        g_ptr_array_add (targets,
                         g_object_new (IDE_TYPE_BUILDCONFIG_BUILD_TARGET,
                                       "command", run_command,
                                       NULL));

    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&targets),
                           g_ptr_array_unref);
}

static GPtrArray *
ide_buildconfig_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *provider,
                                                          GAsyncResult            *result,
                                                          GError                 **error)
{
  GPtrArray *ret;

  g_assert (IDE_IS_BUILDCONFIG_BUILD_TARGET_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
build_target_provider_iface_init (IdeBuildTargetProviderInterface *iface)
{
  iface->get_targets_async = ide_buildconfig_build_target_provider_get_targets_async;
  iface->get_targets_finish = ide_buildconfig_build_target_provider_get_targets_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeBuildconfigBuildTargetProvider,
                               ide_buildconfig_build_target_provider,
                               IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET_PROVIDER,
                                                      build_target_provider_iface_init))

static void
ide_buildconfig_build_target_provider_class_init (IdeBuildconfigBuildTargetProviderClass *klass)
{
}

static void
ide_buildconfig_build_target_provider_init (IdeBuildconfigBuildTargetProvider *self)
{
}
