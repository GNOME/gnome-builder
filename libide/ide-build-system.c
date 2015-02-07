/* ide-build-system.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-build-system.h"
#include "ide-context.h"
#include "ide-device.h"

typedef struct
{
  GFile *project_file;
} IdeBuildSystemPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuildSystem, ide_build_system, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PROJECT_FILE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GFile *
ide_build_system_get_project_file (IdeBuildSystem *system)
{
  IdeBuildSystemPrivate *priv = ide_build_system_get_instance_private (system);

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (system), NULL);

  return priv->project_file;
}

static void
ide_build_system_set_project_file (IdeBuildSystem *system,
                                   GFile          *project_file)
{
  IdeBuildSystemPrivate *priv = ide_build_system_get_instance_private (system);

  g_return_if_fail (IDE_IS_BUILD_SYSTEM (system));
  g_return_if_fail (G_IS_FILE (project_file));

  if (project_file != priv->project_file)
    {
      g_clear_object (&priv->project_file);
      priv->project_file = g_object_ref (project_file);
      g_object_notify_by_pspec (G_OBJECT (system),
                                gParamSpecs [PROP_PROJECT_FILE]);
    }
}

static void
ide_build_system_finalize (GObject *object)
{
  IdeBuildSystem *self = (IdeBuildSystem *)object;
  IdeBuildSystemPrivate *priv = ide_build_system_get_instance_private (self);

  g_clear_object (&priv->project_file);

  G_OBJECT_CLASS (ide_build_system_parent_class)->finalize (object);
}

static void
ide_build_system_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeBuildSystem *self = IDE_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      g_value_set_object (value, ide_build_system_get_project_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_system_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeBuildSystem *self = IDE_BUILD_SYSTEM (object);

  switch (prop_id)
    {
    case PROP_PROJECT_FILE:
      ide_build_system_set_project_file (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_system_class_init (IdeBuildSystemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_build_system_finalize;
  object_class->get_property = ide_build_system_get_property;
  object_class->set_property = ide_build_system_set_property;

  gParamSpecs [PROP_PROJECT_FILE] =
    g_param_spec_object ("project-file",
                         _("Project File"),
                         _("The project file."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PROJECT_FILE,
                                   gParamSpecs [PROP_PROJECT_FILE]);
}

static void
ide_build_system_init (IdeBuildSystem *self)
{
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
  g_return_if_fail (callback);

  ide_object_new_async (IDE_BUILD_SYSTEM_EXTENSION_POINT,
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

  return IDE_BUILD_SYSTEM (ret);
}

/**
 * ide_build_system_get_builder:
 * @system: The #IdeBuildSystem to perform the build.
 * @config: The configuration options for the build.
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
  IdeBuildSystemClass *klass;
  IdeBuilder *ret = NULL;

  g_return_val_if_fail (IDE_IS_BUILD_SYSTEM (system), NULL);
  g_return_val_if_fail (config, NULL);
  g_return_val_if_fail (IDE_IS_DEVICE (device), NULL);

  klass = IDE_BUILD_SYSTEM_GET_CLASS (system);

  if (klass->get_builder)
    ret = klass->get_builder (system, config, device, error);
  else
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_NOT_SUPPORTED,
                 _("%s() is not supported on %s build system."),
                 G_STRFUNC,
                 g_type_name (G_TYPE_FROM_INSTANCE (system)));

  return ret;
}

