/* ide-autotools-build-target-provider.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "ide-autotools-build-target-provider"

#include "ide-autotools-build-target-provider.h"

struct _IdeAutotoolsBuildTargetProvider
{
  IdeObject parent_instance;
};

static void
ide_autotools_build_target_provider_get_targets_async (IdeBuildTargetProvider *provider,
                                                       GCancellable           *cancellable,
                                                       GAsyncReadyCallback     callback,
                                                       gpointer                user_data)
{
  IdeAutotoolsBuildTargetProvider *self = (IdeAutotoolsBuildTargetProvider *)provider;
  g_autoptr(GTask) task = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_TARGET_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_autotools_build_target_provider_get_targets_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_context_get_build_system (context);

  if (!IDE_IS_AUTOTOOLS_BUILD_SYSTEM (build_system))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Not a meson build system, ignoring");
      IDE_EXIT;
    }

  IDE_EXIT;
}

static GPtrArray *
ide_autotools_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *provider,
                                                        GAsyncResult            *result,
                                                        GError                 **error)
{
  g_assert (IDE_IS_AUTOTOOLS_BUILD_TARGET_PROVIDER (provider));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (G_TASK (result), provider));

  return g_task_propagate_pointer (G_TASK (provider), error);
}

static void
build_target_provider_iface_init (IdeBuildTargetProviderInterface *iface)
{
  iface->get_targets_async = ide_autotools_build_target_provider_get_targets_async;
  iface->get_targets_finish = ide_autotools_build_target_provider_get_targets_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeAutotoolsBuildTargetProvider,
                         ide_autotools_build_target_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET_PROVIDER,
                                                build_target_provider_iface_init))

static void
ide_autotools_build_target_provider_class_init (IdeAutotoolsBuildTargetProviderClass *klass)
{
}

static void
ide_autotools_build_target_provider_init (IdeAutotoolsBuildTargetProvider *self)
{
}
