/* gbp-make-build-target-provider.c
 *
 * Copyright 2019 Alex Mitchell
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

#define G_LOG_DOMAIN "gbp-make-build-target-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-make-build-system.h"
#include "gbp-make-build-target.h"
#include "gbp-make-build-target-provider.h"

struct _GbpMakeBuildTargetProvider
{
  IdeObject parent_instance;
};

static const char *expected_make_targets[] = {
  "", "all", "install", "run",
};

static void
gbp_make_build_target_provider_get_targets_async (IdeBuildTargetProvider *provider,
                                                  GCancellable           *cancellable,
                                                  GAsyncReadyCallback     callback,
                                                  gpointer                user_data)
{
  GbpMakeBuildTargetProvider *self = (GbpMakeBuildTargetProvider *)provider;
  g_autoptr(GPtrArray) targets = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_MAKE_BUILD_TARGET_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_make_build_target_provider_get_targets_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_MAKE_BUILD_SYSTEM (build_system))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not a make build system");
      IDE_EXIT;
    }

  /* TODO: It would be nice to actually extract all the make targets from the
   * Makefile. We can possibly do various "print" targets we inject into the
   * Makefile like we do in autotools to get this.
   */
  targets = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = 0; i < G_N_ELEMENTS (expected_make_targets); i++)
    g_ptr_array_add (targets, gbp_make_build_target_new (expected_make_targets[i]));
  ide_task_return_pointer (task, g_steal_pointer (&targets), g_ptr_array_unref);

  IDE_EXIT;
}

static GPtrArray *
gbp_make_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *provider,
                                                  GAsyncResult            *result,
                                                  GError                 **error)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_MAKE_BUILD_TARGET_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  /* transfer full semantics */
  IDE_PTR_ARRAY_CLEAR_FREE_FUNC (ret);

  IDE_RETURN (ret);
}

static void
build_target_provider_iface_init (IdeBuildTargetProviderInterface *iface)
{
  iface->get_targets_async = gbp_make_build_target_provider_get_targets_async;
  iface->get_targets_finish = gbp_make_build_target_provider_get_targets_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMakeBuildTargetProvider, gbp_make_build_target_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET_PROVIDER, build_target_provider_iface_init))

static void
gbp_make_build_target_provider_class_init (GbpMakeBuildTargetProviderClass *klass)
{
}

static void
gbp_make_build_target_provider_init (GbpMakeBuildTargetProvider *self)
{
}
