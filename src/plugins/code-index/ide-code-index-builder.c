/* ide-code-index-builder.c
 *
 * Copyright © 2017 Anoop Chandu <anoopchandu96@gmail.com>
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-code-index-builder"

#include "ide-code-index-builder.h"

struct _IdeCodeIndexBuilder
{
  IdeObject parent;
  IdeCodeIndexService *service;
  IdeCodeIndexIndex *index;
};

enum {
  PROP_0,
  PROP_INDEX,
  PROP_SERVICE,
  N_PROPS
};

G_DEFINE_TYPE (IdeCodeIndexBuilder, ide_code_index_builder, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_code_index_builder_dispose (GObject *object)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;

  g_clear_object (&self->index);
  g_clear_object (&self->service);

  G_OBJECT_CLASS (ide_code_index_builder_parent_class)->dispose (object);
}

static void
ide_code_index_builder_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeCodeIndexBuilder *self = IDE_CODE_INDEX_BUILDER (object);

  switch (prop_id)
    {
    case PROP_INDEX:
      g_value_set_object (value, self->index);
      break;

    case PROP_SERVICE:
      g_value_set_object (value, self->service);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_code_index_builder_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeCodeIndexBuilder *self = IDE_CODE_INDEX_BUILDER (object);

  switch (prop_id)
    {
    case PROP_INDEX:
      self->index = g_value_dup_object (value);
      break;

    case PROP_SERVICE:
      self->service = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_code_index_builder_class_init (IdeCodeIndexBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_code_index_builder_dispose;
  object_class->get_property = ide_code_index_builder_get_property;
  object_class->set_property = ide_code_index_builder_set_property;

  properties [PROP_INDEX] =
    g_param_spec_object ("index",
                         "Index",
                         "The index to update after building sub-indexes",
                         IDE_TYPE_CODE_INDEX_INDEX,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties [PROP_SERVICE] =
    g_param_spec_object ("service",
                         "Service",
                         "The service to query for various build information",
                         IDE_TYPE_CODE_INDEX_SERVICE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_code_index_builder_init (IdeCodeIndexBuilder *self)
{
}

IdeCodeIndexBuilder *
ide_code_index_builder_new (IdeContext          *context,
                            IdeCodeIndexService *service,
                            IdeCodeIndexIndex   *index)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_SERVICE (service), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_INDEX (index), NULL);

  return g_object_new (IDE_TYPE_CODE_INDEX_BUILDER,
                       "context", context,
                       "service", service,
                       "index", index,
                       NULL);
}

void
ide_code_index_builder_drop_caches (IdeCodeIndexBuilder *self)
{
  g_return_if_fail (IDE_IS_CODE_INDEX_BUILDER (self));

  g_warning ("TODO: drop caches");
}

void
ide_code_index_builder_build_async (IdeCodeIndexBuilder *self,
                                    GFile               *directory,
                                    gboolean             recursive,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CODE_INDEX_BUILDER (self));
  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_code_index_builder_build_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

}

gboolean
ide_code_index_builder_build_finish (IdeCodeIndexBuilder  *self,
                                     GAsyncResult         *result,
                                     GError              **error)
{
  g_return_val_if_fail (IDE_IS_CODE_INDEX_BUILDER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
