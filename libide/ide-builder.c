/* ide-builder.c
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

#include "ide-build-result.h"
#include "ide-builder.h"

typedef struct
{
  void *foo;
} IdeBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuilder, ide_builder, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  LAST_PROP
};

//static GParamSpec *gParamSpecs [LAST_PROP];

void
ide_builder_build_async (IdeBuilder           *builder,
                         IdeBuildResult      **result,
                         GCancellable         *cancellable,
                         GAsyncReadyCallback   callback,
                         gpointer              user_data)
{
  IdeBuilderClass *klass;

  g_return_if_fail (IDE_IS_BUILDER (builder));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (result)
    *result = NULL;

  klass = IDE_BUILDER_GET_CLASS (builder);

  if (klass->build_async)
    {
      klass->build_async (builder, result, cancellable, callback, user_data);
      return;
    }

  g_warning (_("%s does not implement build_async()"),
             g_type_name (G_TYPE_FROM_INSTANCE (builder)));

  g_task_report_new_error (builder, callback, user_data,
                           ide_builder_build_async,
                           G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("No implementation of build_async()"));
}

IdeBuildResult *
ide_builder_build_finish (IdeBuilder    *builder,
                          GAsyncResult  *result,
                          GError       **error)
{
  IdeBuilderClass *klass;
  IdeBuildResult *ret = NULL;

  g_return_val_if_fail (IDE_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  klass = IDE_BUILDER_GET_CLASS (builder);

  if (klass->build_finish)
    ret = klass->build_finish (builder, result, error);
  else if (G_IS_TASK (result))
    ret = g_task_propagate_pointer (G_TASK (result), error);

  g_return_val_if_fail (!ret || IDE_IS_BUILD_RESULT (ret), NULL);

  return ret;
}

static void
ide_builder_finalize (GObject *object)
{
  //IdeBuilder *self = (IdeBuilder *)object;
  //IdeBuilderPrivate *priv = ide_builder_get_instance_private (self);

  G_OBJECT_CLASS (ide_builder_parent_class)->finalize (object);
}

static void
ide_builder_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  //IdeBuilder *self = IDE_BUILDER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_builder_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  //IdeBuilder *self = IDE_BUILDER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_builder_class_init (IdeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_builder_finalize;
  object_class->get_property = ide_builder_get_property;
  object_class->set_property = ide_builder_set_property;
}

static void
ide_builder_init (IdeBuilder *self)
{
}
