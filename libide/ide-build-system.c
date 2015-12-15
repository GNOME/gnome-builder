/* ide-build-system.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "ide-build-system.h"
#include "ide-context.h"
#include "ide-device.h"
#include "ide-file.h"
#include "ide-object.h"

typedef struct
{
  GFile *project_file;
} IdeBuildSystemPrivate;

G_DEFINE_INTERFACE (IdeBuildSystem, ide_build_system, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_PROJECT_FILE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

gint
ide_build_system_get_priority (IdeBuildSystem *self)
{
  IdeBuildSystemInterface *iface;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), 0);

  iface = IDE_BUILD_SYSTEM_GET_IFACE (self);

  if (iface->get_priority != NULL)
    return iface->get_priority (self);

  return 0;
}

/**
 * ide_build_system_get_build_flags_async:
 *
 * Asynchronously requests the build flags for a file. For autotools and C based projects, this
 * would be similar to the $CFLAGS variable and is suitable for generating warnings and errors
 * with clang.
 */
void
ide_build_system_get_build_flags_async (IdeBuildSystem      *self,
                                        IdeFile             *file,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_BUILD_SYSTEM (self));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_flags_async)
    return IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_flags_async (self, file, cancellable,
                                                                     callback, user_data);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_pointer (task, NULL, NULL);
}

/**
 * ide_build_system_get_build_flags_finish:
 *
 * Completes an asynchronous request to get the build flags for a file.
 *
 * Returns: (array zero-terminated=1) (transfer full): An array of strings
 *   containing the build flags, or %NULL upon failure and @error is set.
 */
gchar **
ide_build_system_get_build_flags_finish (IdeBuildSystem  *self,
                                         GAsyncResult    *result,
                                         GError         **error)
{
  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (self), NULL);

  if (IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_flags_finish)
    return IDE_BUILD_SYSTEM_GET_IFACE (self)->get_build_flags_finish (self, result, error);

  return g_new0 (gchar*, 1);
}

static void
ide_build_system_default_init (IdeBuildSystemInterface *iface)
{
  properties [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         "Project File",
                         "The project file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface, properties [PROP_PROJECT_FILE]);

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "Context",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface, properties [PROP_CONTEXT]);
}

static gint
sort_priority (gconstpointer a,
               gconstpointer b,
               gpointer      data)
{
  IdeBuildSystem **as = (IdeBuildSystem **)a;
  IdeBuildSystem **bs = (IdeBuildSystem **)b;

  return ide_build_system_get_priority (*as) - ide_build_system_get_priority (*bs);
}

/**
 * ide_build_system_new_async:
 * @context: #IdeBuildSystem
 * @project_file: A #GFile containing the directory or project file.
 * @cancellable: (allow-none): A #GCancellable
 * @callback: A callback to execute upon completion
 * @user_data: User data for @callback.
 *
 * Asynchronously creates a new #IdeBuildSystem instance using the registered
 * #GIOExtensionPoint system. Each extension point will be tried asynchronously
 * by priority until one has been found that supports @project_file.
 *
 * If no build system could be found, then ide_build_system_new_finish() will
 * return %NULL.
 */
void
ide_build_system_new_async (IdeContext          *context,
                            GFile               *project_file,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (G_IS_FILE (project_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_object_new_for_extension_async (IDE_TYPE_BUILD_SYSTEM,
                                      sort_priority, NULL,
                                      G_PRIORITY_DEFAULT,
                                      cancellable,
                                      callback,
                                      user_data,
                                      "context", context,
                                      "project-file", project_file,
                                      NULL);
}

/**
 * ide_build_system_new_finish:
 *
 * Complete an asynchronous call to ide_build_system_new_async().
 *
 * Returns: (transfer full): An #IdeBuildSystem if successful; otherwise
 *   %NULL and @error is set.
 */
IdeBuildSystem *
ide_build_system_new_finish (GAsyncResult  *result,
                             GError       **error)
{
  IdeObject *ret;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = ide_object_new_finish (result, error);

  return ret ? IDE_BUILD_SYSTEM (ret) : NULL;
}

/**
 * ide_build_system_get_builder:
 * @system: The #IdeBuildSystem to perform the build.
 * @config: (nullable): The configuration options for the build.
 * @device: The #IdeDevice the result should be able to run on.
 *
 * This function should return an #IdeBuilder that can be used to perform a
 * build of the project using the configuration specified. @device may be
 * a non-local device, for which cross-compilation may be necessary.
 *
 * Returns: (transfer full): An #IdeBuilder or %NULL and @error is set.
 */
IdeBuilder *
ide_build_system_get_builder (IdeBuildSystem  *system,
                              GKeyFile        *config,
                              IdeDevice       *device,
                              GError         **error)
{
  IdeBuildSystemInterface *iface;
  IdeBuilder *ret = NULL;
  g_autoptr(GKeyFile) local = NULL;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (system), NULL);
  g_return_val_if_fail (IDE_IS_DEVICE (device), NULL);

  if (config == NULL)
    config = local = g_key_file_new ();

  iface = IDE_BUILD_SYSTEM_GET_IFACE (system);

  if (iface->get_builder)
    ret = iface->get_builder (system, config, device, error);
  else
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_NOT_SUPPORTED,
                 _("%s() is not supported on %s build system."),
                 G_STRFUNC,
                 g_type_name (G_TYPE_FROM_INSTANCE (system)));

  return ret;
}

