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
} IdeSymbolNodePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSymbolNode, ide_symbol_node, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_FLAGS,
  PROP_KIND,
  PROP_NAME,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

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
      priv->name = g_value_dup_string (value);
      break;

    case PROP_KIND:
      priv->kind = g_value_get_enum (value);
      break;

    case PROP_FLAGS:
      priv->flags = g_value_get_flags (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_symbol_node_class_init (IdeSymbolNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_symbol_node_finalize;
  object_class->get_property = ide_symbol_node_get_property;
  object_class->set_property = ide_symbol_node_set_property;

  gParamSpecs [PROP_NAME] =
    g_param_spec_string ("name",
                         _("Name"),
                         _("Name"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_KIND] =
    g_param_spec_enum ("kind",
                       _("Kind"),
                       _("Kind"),
                       IDE_TYPE_SYMBOL_KIND,
                       IDE_SYMBOL_NONE,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_FLAGS] =
    g_param_spec_flags ("flags",
                        _("Flags"),
                        _("Flags"),
                        IDE_TYPE_SYMBOL_FLAGS,
                        IDE_SYMBOL_FLAGS_NONE,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
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
