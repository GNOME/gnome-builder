/* ide-foundry-compat.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-foundry-compat"

#include "config.h"

#include "ide-build-manager.h"
#include "ide-build-system.h"
#include "ide-device-manager.h"
#include "ide-config-manager.h"
#include "ide-foundry-compat.h"
#include "ide-run-commands.h"
#include "ide-run-manager.h"
#include "ide-runtime-manager.h"
#include "ide-test-manager.h"
#include "ide-toolchain-manager.h"

static gpointer
ensure_child_typed_borrowed (IdeContext *context,
                             GType       child_type)
{
  gpointer ret;

  if (!IDE_IS_MAIN_THREAD ())
    {
      IDE_BACKTRACE;

      g_error ("A plugin has attempted to access child of type %s on a thread without referencing. "
               "This is not allowed and the application will terminate.",
               g_type_name (child_type));
    }

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONTEXT (context));

  if (!(ret = ide_context_peek_child_typed (context, child_type)))
    {
      g_autoptr(IdeObject) child = NULL;

      if (!ide_context_has_project (context))
        {
          g_critical ("A plugin has attempted to access the %s foundry subsystem before a project has been loaded. "
                      "This is not supported and may cause undesired behavior.",
                      g_type_name (child_type));
        }

      child = ide_object_ensure_child_typed (IDE_OBJECT (context), child_type);
      ret = ide_context_peek_child_typed (context, child_type);
    }

  return ret;
}

static gpointer
get_child_typed_borrowed (IdeContext *context,
                          GType       child_type)
{
  GObject *ret;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONTEXT (context));

  /* We get a full ref to the child, but we want to return a borrowed
   * reference to the manager. Since we're on the main thread, we can
   * guarantee that no destroy will happen (since that has to happen
   * in the main thread).
   */
  if ((ret = ide_object_get_child_typed (IDE_OBJECT (context), child_type)))
    g_object_unref (ret);

  return ret;
}

/**
 * ide_build_manager_from_context:
 * @context: a #IdeContext
 *
 * Returns: (transfer none): an #IdeBuildManager
 */
IdeBuildManager *
ide_build_manager_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ensure_child_typed_borrowed (context, IDE_TYPE_BUILD_MANAGER);
}

/**
 * ide_build_manager_ref_from_context:
 * @context: a #IdeContext
 *
 * Returns: (transfer full): an #IdeBuildManager
 */
IdeBuildManager *
ide_build_manager_ref_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_BUILD_MANAGER);
}

/**
 * ide_build_system_from_context:
 * @context: a #IdeContext
 *
 * Gets the build system for the context. If no build system has been
 * registered, then this returns %NULL.
 *
 * Returns: (transfer none) (nullable): an #IdeBuildSystem
 */
IdeBuildSystem *
ide_build_system_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return get_child_typed_borrowed (context, IDE_TYPE_BUILD_SYSTEM);
}

/**
 * ide_config_manager_from_context:
 * @context: a #IdeContext
 *
 * Returns: (transfer none): an #IdeConfigManager
 */
IdeConfigManager *
ide_config_manager_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ensure_child_typed_borrowed (context, IDE_TYPE_CONFIG_MANAGER);
}

/**
 * ide_device_manager_from_context:
 * @context: a #IdeContext
 *
 * Returns: (transfer none): an #IdeDeviceManager
 */
IdeDeviceManager *
ide_device_manager_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ensure_child_typed_borrowed (context, IDE_TYPE_DEVICE_MANAGER);
}

/**
 * ide_toolchain_manager_from_context:
 * @context: a #IdeContext
 *
 * Returns: (transfer none): an #IdeToolchainManager
 */
IdeToolchainManager *
ide_toolchain_manager_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ensure_child_typed_borrowed (context, IDE_TYPE_TOOLCHAIN_MANAGER);
}

/**
 * ide_run_manager_from_context:
 * @context: a #IdeContext
 *
 * Returns: (transfer none): an #IdeRunManager
 */
IdeRunManager *
ide_run_manager_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ensure_child_typed_borrowed (context, IDE_TYPE_RUN_MANAGER);
}

/**
 * ide_runtime_manager_from_context:
 * @context: a #IdeContext
 *
 * Returns: (transfer none): an #IdeRuntimeManager
 */
IdeRuntimeManager *
ide_runtime_manager_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ensure_child_typed_borrowed (context, IDE_TYPE_RUNTIME_MANAGER);
}

/**
 * ide_test_manager_from_context:
 * @context: a #IdeContext
 *
 * Returns: (transfer none): an #IdeTestManager
 */
IdeTestManager *
ide_test_manager_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ensure_child_typed_borrowed (context, IDE_TYPE_TEST_MANAGER);
}

/**
 * ide_run_commands_from_context:
 * @context: an #IdeContext
 *
 * Gets the default #IdeRunCommands instance for @context.
 *
 * Returns: (transfer none): an #IdeRunCommands
 */
IdeRunCommands *
ide_run_commands_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ensure_child_typed_borrowed (context, IDE_TYPE_RUN_COMMANDS);
}
