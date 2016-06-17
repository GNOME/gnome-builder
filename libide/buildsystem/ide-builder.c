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

#include <glib/gi18n.h>

#include "ide-build-result.h"
#include "ide-builder.h"
#include "ide-configuration.h"

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

  if (g_set_object (&priv->configuration, configuration))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONFIGURATION]);
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

/**
 * ide_builder_build_async:
 * @result: (out) (transfer none): A location for an #IdeBuildResult.
 */
void
ide_builder_build_async (IdeBuilder           *builder,
                         IdeBuilderBuildFlags  flags,
                         IdeBuildResult      **result,
                         GCancellable         *cancellable,
                         GAsyncReadyCallback   callback,
                         gpointer              user_data)
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

  object_class->finalize = ide_builder_finalize;
  object_class->get_property = ide_builder_get_property;
  object_class->set_property = ide_builder_set_property;

  klass->build_async = ide_builder_real_build_async;
  klass->build_finish = ide_builder_real_build_finish;

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
