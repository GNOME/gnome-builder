/* ide-builder.c
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

#define G_LOG_DOMAIN "ide-builder"

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buildsystem/ide-build-result.h"
#include "buildsystem/ide-builder.h"
#include "buildsystem/ide-configuration.h"

typedef struct
{
  IdeConfiguration *configuration;
} IdeBuilderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeBuilder, ide_builder, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONFIGURATION,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

/**
 * ide_builder_get_configuration:
 * @self: An #IdeBuilder.
 *
 * Gets the configuration to use for the builder.
 *
 * Returns: (transfer none): An #IdeConfiguration.
 */
IdeConfiguration *
ide_builder_get_configuration (IdeBuilder *self)
{
  IdeBuilderPrivate *priv = ide_builder_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILDER (self), NULL);

  return priv->configuration;
}

static void
ide_builder_set_configuration (IdeBuilder       *self,
                               IdeConfiguration *configuration)
{
  IdeBuilderPrivate *priv = ide_builder_get_instance_private (self);

  g_assert (IDE_IS_BUILDER (self));
  g_assert (!configuration || IDE_IS_CONFIGURATION (configuration));
  g_assert (priv->configuration == NULL);

  /* Make a copy of the configuration so that we do not need to worry
   * about the user modifying the configuration while our bulid is
   * active (and may be running in another thread).
   *
   * When the dirty bit is cleared from a successful build, the
   * configuration will propagate that to the original build
   * configuration.
   */

  priv->configuration = ide_configuration_snapshot (configuration);
}

static void
ide_builder_real_build_async (IdeBuilder            *self,
                              IdeBuilderBuildFlags   flags,
                              IdeBuildResult       **result,
                              GCancellable          *cancellable,
                              GAsyncReadyCallback    callback,
                              gpointer               user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_BUILDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (!result || *result == NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_builder_real_build_async);

  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           _("%s does not support building"),
                           G_OBJECT_TYPE_NAME (self));
}

static IdeBuildResult *
ide_builder_real_build_finish (IdeBuilder    *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_assert (IDE_IS_BUILDER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_builder_real_get_build_targets_async (IdeBuilder          *self,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_builder_real_get_build_targets_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "build targets not supported for %s",
                           G_OBJECT_TYPE_NAME (self));
}

static GPtrArray *
ide_builder_real_get_build_targets_finish (IdeBuilder    *self,
                                           GAsyncResult  *result,
                                           GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_builder_real_get_build_flags_async (IdeBuilder          *self,
                                        IdeFile             *file,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_builder_real_get_build_flags_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "build flags not supported for %s",
                           G_OBJECT_TYPE_NAME (self));
}

static gchar **
ide_builder_real_get_build_flags_finish (IdeBuilder    *self,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_builder_real_install_async (IdeBuilder          *self,
                                IdeBuildResult     **build_result,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_builder_real_install_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "install not supported for %s",
                           G_OBJECT_TYPE_NAME (self));
}

static IdeBuildResult *
ide_builder_real_install_finish (IdeBuilder    *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * ide_builder_build_async:
 * @self: An #IdeBuilder
 * @flags: build flags for the build
 * @result: (out) (transfer full) (nullable): A location for an #IdeBuildResult
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: A callback to complete the async operation
 * @user_data: user data for @callback
 *
 * This function requests the #IdeBuilder to asynchronously begin building
 * the project using the flags specified. By default, the builders will try
 * to perform incremental builds.
 *
 * See ide_builder_build_finish() to complete the request.
 */
void
ide_builder_build_async (IdeBuilder            *builder,
                         IdeBuilderBuildFlags   flags,
                         IdeBuildResult       **result,
                         GCancellable          *cancellable,
                         GAsyncReadyCallback    callback,
                         gpointer               user_data)
{
  g_return_if_fail (IDE_IS_BUILDER (builder));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (result != NULL)
    *result = NULL;

  IDE_BUILDER_GET_CLASS (builder)->build_async (builder, flags, result, cancellable, callback, user_data);
}

/**
 * ide_builder_build_finish:
 *
 * Completes an asynchronous request to build the project.
 *
 * Returns: (transfer full): An #IdeBuildResult or %NULL upon failure.
 */
IdeBuildResult *
ide_builder_build_finish (IdeBuilder    *builder,
                          GAsyncResult  *result,
                          GError       **error)
{
  IdeBuildResult *ret = NULL;

  g_return_val_if_fail (IDE_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = IDE_BUILDER_GET_CLASS (builder)->build_finish (builder, result, error);

  g_return_val_if_fail (!ret || IDE_IS_BUILD_RESULT (ret), NULL);

  return ret;
}

static void
ide_builder_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeBuilder *self = IDE_BUILDER(object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      g_value_set_object (value, ide_builder_get_configuration (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_builder_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeBuilder *self = IDE_BUILDER(object);

  switch (prop_id)
    {
    case PROP_CONFIGURATION:
      ide_builder_set_configuration (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_builder_constructed (GObject *object)
{
  G_OBJECT_CLASS (ide_builder_parent_class)->constructed (object);

#ifdef IDE_ENABLE_TRACE
  {
    IdeContext *context = ide_object_get_context (IDE_OBJECT (object));
    g_assert (IDE_IS_CONTEXT (context));
  }
#endif
}

static void
ide_builder_finalize (GObject *object)
{
  IdeBuilder *self = (IdeBuilder *)object;
  IdeBuilderPrivate *priv = ide_builder_get_instance_private (self);

  g_clear_object (&priv->configuration);

  G_OBJECT_CLASS (ide_builder_parent_class)->finalize (object);
}

static void
ide_builder_class_init (IdeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_builder_constructed;
  object_class->finalize = ide_builder_finalize;
  object_class->get_property = ide_builder_get_property;
  object_class->set_property = ide_builder_set_property;

  klass->build_async = ide_builder_real_build_async;
  klass->build_finish = ide_builder_real_build_finish;
  klass->install_async = ide_builder_real_install_async;
  klass->install_finish = ide_builder_real_install_finish;
  klass->get_build_flags_async = ide_builder_real_get_build_flags_async;
  klass->get_build_flags_finish = ide_builder_real_get_build_flags_finish;
  klass->get_build_targets_async = ide_builder_real_get_build_targets_async;
  klass->get_build_targets_finish = ide_builder_real_get_build_targets_finish;

  properties [PROP_CONFIGURATION] =
    g_param_spec_object ("configuration",
                         "Configuration",
                         "Configuration",
                         IDE_TYPE_CONFIGURATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_builder_init (IdeBuilder *self)
{
}

/**
 * ide_builder_install_async:
 * @self: An #IdeBuilder
 * @result: (out) (transfer full) (nullable): A location for an #IdeBuildResult
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: A callback to complete the async operation
 * @user_data: user data for @callback
 *
 * This function requests the #IdeBuilder to asynchronously begin installing
 * the project.
 *
 * See ide_builder_install_finish() to complete the request.
 */
void
ide_builder_install_async (IdeBuilder           *self,
                           IdeBuildResult      **result,
                           GCancellable         *cancellable,
                           GAsyncReadyCallback   callback,
                           gpointer              user_data)
{
  g_return_if_fail (IDE_IS_BUILDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (result != NULL)
    *result = NULL;

  IDE_BUILDER_GET_CLASS (self)->install_async (self, result, cancellable, callback, user_data);
}

/**
 * ide_builder_install_finish:
 *
 * Completes an asynchronous call to ide_builder_install_async().
 *
 * Returns: (transfer full): An #IdeBuildResult.
 */
IdeBuildResult *
ide_builder_install_finish (IdeBuilder    *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  IdeBuildResult *ret;

  g_return_val_if_fail (IDE_IS_BUILDER (self), NULL);

  ret = IDE_BUILDER_GET_CLASS (self)->install_finish (self, result, error);

  g_return_val_if_fail (!ret || IDE_IS_BUILD_RESULT (ret), NULL);

  return ret;
}

void
ide_builder_get_build_targets_async (IdeBuilder          *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_return_if_fail (IDE_IS_BUILDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUILDER_GET_CLASS (self)->get_build_targets_async (self, cancellable, callback, user_data);
}

/**
 * ide_builder_get_build_targets_finish:
 * @self: An #IdeBuilder
 * @result: A #GAsyncResult provided to the async callback
 * @error: A location for a #GError or %NULL
 *
 * Completes an async operation to ide_builder_get_build_targets_async().
 *
 * Returns: (transfer container) (element-type Ide.BuildTarget): A #GPtrArray of the
 *   build targets or %NULL upon failure and @error is set.
 */
GPtrArray *
ide_builder_get_build_targets_finish (IdeBuilder    *self,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  g_return_val_if_fail (IDE_IS_BUILDER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return IDE_BUILDER_GET_CLASS (self)->get_build_targets_finish (self, result, error);
}

void
ide_builder_get_build_flags_async (IdeBuilder          *self,
                                   IdeFile             *file,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_return_if_fail (IDE_IS_BUILDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUILDER_GET_CLASS (self)->get_build_flags_async (self, file, cancellable, callback, user_data);
}

/**
 * ide_builder_get_build_flags_finish:
 * @self: An #IdeBuilder
 * @result: A #GAsyncResult provided to the async callback
 * @error: A location for a #GError, or %NULL
 *
 * Completes the async operation to ide_builder_get_build_flags_async()
 *
 * Returns: (transfer full): A newly allocated %NULL terminated array of strings,
 *   or %NULL upon failure.
 */
gchar **
ide_builder_get_build_flags_finish (IdeBuilder    *self,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  g_return_val_if_fail (IDE_IS_BUILDER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return IDE_BUILDER_GET_CLASS (self)->get_build_flags_finish (self, result, error);
}
