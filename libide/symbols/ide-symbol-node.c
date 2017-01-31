/* ide-symbol-node.c
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

#include "ide-enums.h"
#include "ide-symbol.h"
#include "ide-symbol-node.h"

typedef struct
{
  gchar          *name;
  IdeSymbolFlags  flags;
  IdeSymbolKind   kind;
  guint           use_markup : 1;
} IdeSymbolNodePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSymbolNode, ide_symbol_node, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_FLAGS,
  PROP_KIND,
  PROP_NAME,
  PROP_USE_MARKUP,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_symbol_node_real_get_location_async (IdeSymbolNode       *self,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_symbol_node_get_location_async);
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Unsupported operation on symbol node");
}

static IdeSourceLocation *
ide_symbol_node_real_get_location_finish (IdeSymbolNode  *self,
                                          GAsyncResult   *result,
                                          GError        **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_symbol_node_finalize (GObject *object)
{
  IdeSymbolNode *self = (IdeSymbolNode *)object;
  IdeSymbolNodePrivate *priv = ide_symbol_node_get_instance_private (self);

  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (ide_symbol_node_parent_class)->finalize (object);
}

static void
ide_symbol_node_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeSymbolNode *self = IDE_SYMBOL_NODE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, ide_symbol_node_get_name (self));
      break;

    case PROP_KIND:
      g_value_set_enum (value, ide_symbol_node_get_kind (self));
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, ide_symbol_node_get_flags (self));
      break;

    case PROP_USE_MARKUP:
      g_value_set_boolean (value, ide_symbol_node_get_use_markup (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_symbol_node_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeSymbolNode *self = IDE_SYMBOL_NODE (object);
  IdeSymbolNodePrivate *priv = ide_symbol_node_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_NAME:
      g_free (priv->name);
      priv->name = g_value_dup_string (value);
      break;

    case PROP_KIND:
      priv->kind = g_value_get_enum (value);
      break;

    case PROP_FLAGS:
      priv->flags = g_value_get_flags (value);
      break;

    case PROP_USE_MARKUP:
      priv->use_markup = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_symbol_node_class_init (IdeSymbolNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->get_location_async = ide_symbol_node_real_get_location_async;
  klass->get_location_finish = ide_symbol_node_real_get_location_finish;

  object_class->finalize = ide_symbol_node_finalize;
  object_class->get_property = ide_symbol_node_get_property;
  object_class->set_property = ide_symbol_node_set_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_KIND] =
    g_param_spec_enum ("kind",
                       "Kind",
                       "Kind",
                       IDE_TYPE_SYMBOL_KIND,
                       IDE_SYMBOL_NONE,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FLAGS] =
    g_param_spec_flags ("flags",
                        "Flags",
                        "Flags",
                        IDE_TYPE_SYMBOL_FLAGS,
                        IDE_SYMBOL_FLAGS_NONE,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_MARKUP] =
    g_param_spec_boolean ("use-markup",
                          "use-markup",
                          "Use markup",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_symbol_node_init (IdeSymbolNode *self)
{
}

const gchar *
ide_symbol_node_get_name (IdeSymbolNode *self)
{
  IdeSymbolNodePrivate *priv = ide_symbol_node_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SYMBOL_NODE (self), NULL);

  return priv->name;
}

IdeSymbolFlags
ide_symbol_node_get_flags (IdeSymbolNode *self)
{
  IdeSymbolNodePrivate *priv = ide_symbol_node_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SYMBOL_NODE (self), IDE_SYMBOL_FLAGS_NONE);

  return priv->flags;
}

IdeSymbolKind
ide_symbol_node_get_kind (IdeSymbolNode *self)
{
  IdeSymbolNodePrivate *priv = ide_symbol_node_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SYMBOL_NODE (self), IDE_SYMBOL_NONE);

  return priv->kind;
}

gboolean
ide_symbol_node_get_use_markup (IdeSymbolNode *self)
{
  IdeSymbolNodePrivate *priv = ide_symbol_node_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SYMBOL_NODE (self), FALSE);

  return priv->use_markup;
}

void
ide_symbol_node_get_location_async (IdeSymbolNode       *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SYMBOL_NODE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SYMBOL_NODE_GET_CLASS (self)->get_location_async (self, cancellable, callback, user_data);
}

/**
 * ide_symbol_node_get_location_finish:
 *
 * Completes the request to gets the location for the symbol node.
 *
 * Returns: (transfer full) (nullable): An #IdeSourceLocation or %NULL.
 */
IdeSourceLocation *
ide_symbol_node_get_location_finish (IdeSymbolNode  *self,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  g_return_val_if_fail (IDE_IS_SYMBOL_NODE (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_SYMBOL_NODE_GET_CLASS (self)->get_location_finish (self, result, error);
}
