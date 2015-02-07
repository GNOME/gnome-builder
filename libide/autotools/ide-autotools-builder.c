/* ide-autotools-builder.c
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

#include "ide-autotools-build-task.h"
#include "ide-autotools-builder.h"
#include "ide-build-result.h"
#include "ide-context.h"
#include "ide-device.h"
#include "ide-project.h"

typedef struct
{
  GKeyFile  *config;
  IdeDevice *device;
} IdeAutotoolsBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeAutotoolsBuilder, ide_autotools_builder,
                            IDE_TYPE_BUILDER)

enum {
  PROP_0,
  PROP_CONFIG,
  PROP_DEVICE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GKeyFile *
ide_autotools_builder_get_config (IdeAutotoolsBuilder *builder)
{
  IdeAutotoolsBuilderPrivate *priv;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILDER (builder), NULL);

  priv = ide_autotools_builder_get_instance_private (builder);

  return priv->config;
}

static void
ide_autotools_builder_set_config (IdeAutotoolsBuilder *builder,
                                  GKeyFile            *config)
{
  IdeAutotoolsBuilderPrivate *priv;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILDER (builder));

  priv = ide_autotools_builder_get_instance_private (builder);

  if (priv->config != config)
    {
      g_clear_pointer (&priv->config, g_key_file_unref);
      if (config)
        priv->config = g_key_file_ref (config);
      g_object_notify_by_pspec (G_OBJECT (builder),
                                gParamSpecs [PROP_CONFIG]);
    }
}

IdeDevice *
ide_autotools_builder_get_device (IdeAutotoolsBuilder *builder)
{
  IdeAutotoolsBuilderPrivate *priv;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILDER (builder), NULL);

  priv = ide_autotools_builder_get_instance_private (builder);

  return priv->device;
}

static void
ide_autotools_builder_set_device (IdeAutotoolsBuilder *builder,
                                  IdeDevice           *device)
{
  IdeAutotoolsBuilderPrivate *priv;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILDER (builder));
  g_return_if_fail (!device || IDE_IS_DEVICE (device));

  priv = ide_autotools_builder_get_instance_private (builder);

  if (priv->device != device)
    if (g_set_object (&priv->device, device))
      g_object_notify_by_pspec (G_OBJECT (builder), gParamSpecs [PROP_DEVICE]);
}

static void
ide_autotools_builder_build_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  IdeAutotoolsBuildTask *build_result = (IdeAutotoolsBuildTask *)object;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILD_TASK (build_result));
  g_return_if_fail (G_IS_TASK (task));

  if (!ide_autotools_build_task_execute_finish (build_result, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, g_object_ref (build_result), g_object_unref);
}

static void
ide_autotools_builder_build_async (IdeBuilder           *builder,
                                   IdeBuildResult      **result,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  IdeAutotoolsBuilderPrivate *priv;
  IdeAutotoolsBuilder *self = (IdeAutotoolsBuilder *)builder;
  g_autoptr(IdeAutotoolsBuildTask) build_result = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(gchar) build_dir = NULL;
  g_autoptr(GFile) directory = NULL;
  const gchar *device_id;
  const gchar *project_name;
  const gchar *root_build_dir;
  const gchar *system_type;
  IdeContext *context;
  IdeDevice *device;
  IdeProject *project;

  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILDER (builder));
  g_return_if_fail (IDE_IS_AUTOTOOLS_BUILDER (self));

  priv = ide_autotools_builder_get_instance_private (self);

  task = g_task_new (self, cancellable, callback, user_data);

  device = ide_autotools_builder_get_device (self);
  device_id = ide_device_get_id (device);
  system_type = ide_device_get_system_type (device);

  context = ide_object_get_context (IDE_OBJECT (builder));
  root_build_dir = ide_context_get_root_build_dir (context);

  project = ide_context_get_project (context);
  project_name = ide_project_get_name (project);

  build_dir = g_build_filename (root_build_dir,
                                project_name,
                                device_id,
                                system_type,
                                NULL);
  directory = g_file_new_for_path (build_dir);

  build_result = g_object_new (IDE_TYPE_AUTOTOOLS_BUILD_TASK,
                               "context", context,
                               "config", priv->config,
                               "device", device,
                               "directory", directory,
                               NULL);

  if (result)
    *result = g_object_ref (build_result);

  ide_autotools_build_task_execute_async (build_result,
                                          cancellable,
                                          ide_autotools_builder_build_cb,
                                          g_object_ref (task));
}

static IdeBuildResult *
ide_autotools_builder_build_finish (IdeBuilder    *builder,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_AUTOTOOLS_BUILDER (builder), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_autotools_builder_finalize (GObject *object)
{
  IdeAutotoolsBuilder *self = (IdeAutotoolsBuilder *)object;
  IdeAutotoolsBuilderPrivate *priv = ide_autotools_builder_get_instance_private (self);

  g_clear_object (&priv->config);
  g_clear_object (&priv->device);

  G_OBJECT_CLASS (ide_autotools_builder_parent_class)->finalize (object);
}

static void
ide_autotools_builder_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeAutotoolsBuilder *self = IDE_AUTOTOOLS_BUILDER (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      g_value_set_boxed (value, ide_autotools_builder_get_config (self));
      break;

    case PROP_DEVICE:
      g_value_set_object (value, ide_autotools_builder_get_device (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_builder_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeAutotoolsBuilder *self = IDE_AUTOTOOLS_BUILDER (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      ide_autotools_builder_set_config (self, g_value_get_boxed (value));
      break;

    case PROP_DEVICE:
      ide_autotools_builder_set_device (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_builder_class_init (IdeAutotoolsBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeBuilderClass *builder_class = IDE_BUILDER_CLASS (klass);

  object_class->finalize = ide_autotools_builder_finalize;
  object_class->get_property = ide_autotools_builder_get_property;
  object_class->set_property = ide_autotools_builder_set_property;

  builder_class->build_async = ide_autotools_builder_build_async;
  builder_class->build_finish = ide_autotools_builder_build_finish;

  gParamSpecs [PROP_CONFIG] =
    g_param_spec_boxed ("config",
                        _("Config"),
                        _("The configuration for the build."),
                        G_TYPE_KEY_FILE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CONFIG,
                                   gParamSpecs [PROP_CONFIG]);

  gParamSpecs [PROP_DEVICE] =
    g_param_spec_object ("device",
                         _("Device"),
                         _("The device to build for."),
                         IDE_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DEVICE,
                                   gParamSpecs [PROP_DEVICE]);
}

static void
ide_autotools_builder_init (IdeAutotoolsBuilder *self)
{
}
