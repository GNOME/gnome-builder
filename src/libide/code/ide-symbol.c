/* ide-symbol.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-symbol"

#include "config.h"

#include "ide-code-enums.h"
#include "ide-location.h"
#include "ide-symbol.h"

typedef struct
{
  IdeSymbolKind   kind;
  IdeSymbolFlags  flags;
  gchar          *name;
  IdeLocation    *location;
  IdeLocation    *header_location;
} IdeSymbolPrivate;

enum {
  PROP_0,
  PROP_KIND,
  PROP_FLAGS,
  PROP_NAME,
  PROP_LOCATION,
  PROP_HEADER_LOCATION,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeSymbol, ide_symbol, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_symbol_finalize (GObject *object)
{
  IdeSymbol *self = (IdeSymbol *)object;
  IdeSymbolPrivate *priv = ide_symbol_get_instance_private (self);

  g_clear_pointer (&priv->name, g_free);
  g_clear_object (&priv->location);
  g_clear_object (&priv->header_location);

  G_OBJECT_CLASS (ide_symbol_parent_class)->finalize (object);
}

static void
ide_symbol_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeSymbol *self = IDE_SYMBOL (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, ide_symbol_get_kind (self));
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, ide_symbol_get_flags (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_symbol_get_name (self));
      break;

    case PROP_LOCATION:
      g_value_set_object (value, ide_symbol_get_location (self));
      break;

    case PROP_HEADER_LOCATION:
      g_value_set_object (value, ide_symbol_get_header_location (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_symbol_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeSymbol *self = IDE_SYMBOL (object);
  IdeSymbolPrivate *priv = ide_symbol_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_KIND:
      priv->kind = g_value_get_enum (value);
      break;

    case PROP_FLAGS:
      priv->flags = g_value_get_flags (value);
      break;

    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    case PROP_LOCATION:
      priv->location = g_value_dup_object (value);
      break;

    case PROP_HEADER_LOCATION:
      priv->header_location = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_symbol_class_init (IdeSymbolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_symbol_finalize;
  object_class->get_property = ide_symbol_get_property;
  object_class->set_property = ide_symbol_set_property;

  properties [PROP_KIND] =
    g_param_spec_enum ("kind",
                       "Kind",
                       "The kind of symbol",
                       IDE_TYPE_SYMBOL_KIND,
                       IDE_SYMBOL_KIND_NONE,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FLAGS] =
    g_param_spec_flags ("flags",
                        "Flags",
                        "The symbol flags",
                        IDE_TYPE_SYMBOL_FLAGS,
                        IDE_SYMBOL_FLAGS_NONE,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name of the symbol",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LOCATION] =
    g_param_spec_object ("location",
                         "Location",
                         "The location for the symbol",
                         IDE_TYPE_LOCATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_HEADER_LOCATION] =
    g_param_spec_object ("header-location",
                         "Header Location",
                         "The header location for the symbol",
                         IDE_TYPE_LOCATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_symbol_init (IdeSymbol *self)
{
}

IdeSymbolKind
ide_symbol_get_kind (IdeSymbol *self)
{
  IdeSymbolPrivate *priv = ide_symbol_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SYMBOL (self), 0);

  return priv->kind;
}

IdeSymbolFlags
ide_symbol_get_flags (IdeSymbol *self)
{
  IdeSymbolPrivate *priv = ide_symbol_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SYMBOL (self), 0);

  return priv->flags;
}

const gchar *
ide_symbol_get_name (IdeSymbol *self)
{
  IdeSymbolPrivate *priv = ide_symbol_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SYMBOL (self), NULL);

  return priv->name;
}

/**
 * ide_symbol_get_location:
 * @self: a #IdeSymbol
 *
 * Gets the location, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeLocation or %NULL
 */
IdeLocation *
ide_symbol_get_location (IdeSymbol *self)
{
  IdeSymbolPrivate *priv = ide_symbol_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SYMBOL (self), NULL);

  return priv->location;
}

/**
 * ide_symbol_get_header_location:
 * @self: a #IdeSymbol
 *
 * Gets the header location, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeLocation or %NULL
 */
IdeLocation *
ide_symbol_get_header_location (IdeSymbol *self)
{
  IdeSymbolPrivate *priv = ide_symbol_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SYMBOL (self), NULL);

  return priv->header_location;
}

const gchar *
ide_symbol_kind_get_icon_name (IdeSymbolKind kind)
{
  const gchar *icon_name = NULL;

  switch (kind)
    {
    case IDE_SYMBOL_KIND_ALIAS:
    case IDE_SYMBOL_KIND_TYPE_PARAM:
      icon_name = "lang-typedef-symbolic";
      break;

    case IDE_SYMBOL_KIND_INTERFACE:
    case IDE_SYMBOL_KIND_OBJECT:
    case IDE_SYMBOL_KIND_CLASS:
      icon_name = "lang-class-symbolic";
      break;

    case IDE_SYMBOL_KIND_ENUM:
      icon_name = "lang-enum-symbolic";
      break;

    case IDE_SYMBOL_KIND_ENUM_VALUE:
      icon_name = "lang-enum-value-symbolic";
      break;

    case IDE_SYMBOL_KIND_CONSTRUCTOR:
    case IDE_SYMBOL_KIND_FUNCTION:
      icon_name = "lang-function-symbolic";
      break;

    case IDE_SYMBOL_KIND_MODULE:
    case IDE_SYMBOL_KIND_PACKAGE:
    case IDE_SYMBOL_KIND_HEADER:
    case IDE_SYMBOL_KIND_FILE:
      icon_name = "lang-include-symbolic";
      break;

    case IDE_SYMBOL_KIND_MACRO:
      icon_name = "lang-define-symbolic";
      break;

    case IDE_SYMBOL_KIND_METHOD:
      icon_name = "lang-method-symbolic";
      break;

    case IDE_SYMBOL_KIND_NAMESPACE:
      icon_name = "lang-namespace-symbolic";
      break;

    case IDE_SYMBOL_KIND_STRUCT:
      icon_name = "lang-struct-symbolic";
      break;

    case IDE_SYMBOL_KIND_PROPERTY:
    case IDE_SYMBOL_KIND_FIELD:
      icon_name = "lang-struct-field-symbolic";
      break;

    case IDE_SYMBOL_KIND_SCALAR:
    case IDE_SYMBOL_KIND_VARIABLE:
      icon_name = "lang-variable-symbolic";
      break;

    case IDE_SYMBOL_KIND_UNION:
      icon_name = "lang-union-symbolic";
      break;

    case IDE_SYMBOL_KIND_TEMPLATE:
    case IDE_SYMBOL_KIND_STRING:
      icon_name = "completion-snippet-symbolic";
      break;

    case IDE_SYMBOL_KIND_EVENT:
    case IDE_SYMBOL_KIND_OPERATOR:
    case IDE_SYMBOL_KIND_ARRAY:
    case IDE_SYMBOL_KIND_BOOLEAN:
    case IDE_SYMBOL_KIND_CONSTANT:
    case IDE_SYMBOL_KIND_NUMBER:
    case IDE_SYMBOL_KIND_NONE:
    case IDE_SYMBOL_KIND_KEYWORD:
    case IDE_SYMBOL_KIND_LAST:
      icon_name = NULL;
      break;

    case IDE_SYMBOL_KIND_UI_ATTRIBUTES:
      icon_name = "ui-attributes-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_CHILD:
      icon_name = "ui-child-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_ITEM:
      icon_name = "ui-item-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_MENU:
      icon_name = "ui-menu-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_OBJECT:
      icon_name = "ui-object-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_PACKING:
      icon_name = "ui-packing-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_PROPERTY:
      icon_name = "ui-property-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_SECTION:
      icon_name = "ui-section-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_SIGNAL:
      icon_name = "ui-signal-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_STYLE:
      icon_name = "ui-style-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_SUBMENU:
      icon_name = "ui-submenu-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_TEMPLATE:
      icon_name = "ui-template-symbolic";
      break;

    case IDE_SYMBOL_KIND_XML_ATTRIBUTE:
      icon_name = "xml-attribute-symbolic";
      break;

    case IDE_SYMBOL_KIND_XML_CDATA:
      icon_name = "xml-cdata-symbolic";
      break;

    case IDE_SYMBOL_KIND_XML_COMMENT:
      icon_name = "xml-comment-symbolic";
      break;

    case IDE_SYMBOL_KIND_XML_DECLARATION:
      icon_name = "xml-declaration-symbolic";
      break;

    case IDE_SYMBOL_KIND_XML_ELEMENT:
      icon_name = "xml-element-symbolic";
      break;

    case IDE_SYMBOL_KIND_UI_MENU_ATTRIBUTE:
    case IDE_SYMBOL_KIND_UI_STYLE_CLASS:
      icon_name = NULL;
      break;

    default:
      icon_name = NULL;
      break;
    }

  return icon_name;
}

/**
 * ide_symbol_kind_get_gicon:
 * @kind: a #IdeSymbolKind
 *
 * Gets a #GIcon to represent the symbol kind.
 *
 * You may only call this from the main (GTK) thread.
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 */
GIcon *
ide_symbol_kind_get_gicon (IdeSymbolKind kind)
{
  const char *icon_name;
  GIcon *ret = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());

  if ((icon_name = ide_symbol_kind_get_icon_name (kind)))
    {
      static GHashTable *cached;

      if (cached == NULL)
        cached = g_hash_table_new (NULL, NULL);

      ret = g_hash_table_lookup (cached, icon_name);

      if (ret == NULL)
        {
          ret = g_themed_icon_new (icon_name);
          g_hash_table_insert (cached, (char *)icon_name, ret);
        }
    }

  return ret;
}

/**
 * ide_symbol_to_variant:
 * @self: a #IdeSymbol
 *
 * This converts the symbol to a #GVariant that is suitable for passing
 * across an IPC boundary.
 *
 * This function will never return a floating reference.
 *
 * Returns: (transfer full): a #GVariant
 */
GVariant *
ide_symbol_to_variant (IdeSymbol *self)
{
  IdeSymbolPrivate *priv = ide_symbol_get_instance_private (self);
  GVariantBuilder builder;

  g_return_val_if_fail (self != NULL, NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

  g_variant_builder_add_parsed (&builder, "{%s,<%i>}", "kind", priv->kind);
  g_variant_builder_add_parsed (&builder, "{%s,<%i>}", "flags", priv->flags);
  g_variant_builder_add_parsed (&builder, "{%s,<%s>}", "name", priv->name);

  if (priv->location)
    {
      g_autoptr(GVariant) v = ide_location_to_variant (priv->location);
      g_variant_builder_add_parsed (&builder, "{%s,%v}", "location", v);
    }

  if (priv->header_location)
    {
      g_autoptr(GVariant) v = ide_location_to_variant (priv->header_location);
      g_variant_builder_add_parsed (&builder, "{%s,%v}", "header-location", v);
    }

  return g_variant_take_ref (g_variant_builder_end (&builder));
}

IdeSymbol *
ide_symbol_new_from_variant (GVariant *variant)
{
  g_autoptr(GVariant) unboxed = NULL;
  g_autoptr(GVariant) vdecl = NULL;
  g_autoptr(GVariant) vdef = NULL;
  g_autoptr(IdeLocation) decl = NULL;
  g_autoptr(IdeLocation) def = NULL;
  const gchar *name;
  IdeSymbolKind kind;
  IdeSymbolFlags flags;
  IdeSymbol *self;
  GVariantDict dict;

  if (variant == NULL)
    return NULL;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
    variant = unboxed = g_variant_get_variant (variant);

  if (!g_variant_is_of_type (variant, G_VARIANT_TYPE_VARDICT))
    return NULL;

  g_variant_dict_init (&dict, variant);

  if (!g_variant_dict_lookup (&dict, "kind", "i", &kind))
    kind = 0;

  if (!g_variant_dict_lookup (&dict, "flags", "i", &flags))
    flags = 0;

  if (!g_variant_dict_lookup (&dict, "name", "&s", &name))
    name = NULL;

  vdef = g_variant_dict_lookup_value (&dict, "location", NULL);
  vdecl = g_variant_dict_lookup_value (&dict, "header-location", NULL);

  decl = ide_location_new_from_variant (vdecl);
  def = ide_location_new_from_variant (vdef);

  self = ide_symbol_new (name, kind, flags, decl, def);

  g_variant_dict_clear (&dict);

  return self;
}

/**
 * ide_symbol_new:
 * @location: (nullable):
 * @header_location: (nullable):
 *
 * Returns: (transfer full): an #IdeSymbol
 */
IdeSymbol *
ide_symbol_new (const gchar    *name,
                IdeSymbolKind   kind,
                IdeSymbolFlags  flags,
                IdeLocation    *location,
                IdeLocation    *header_location)
{
  g_return_val_if_fail (!location || IDE_IS_LOCATION (location), NULL);
  g_return_val_if_fail (!header_location || IDE_IS_LOCATION (header_location), NULL);

  return g_object_new (IDE_TYPE_SYMBOL,
                       "name", name,
                       "kind", kind,
                       "flags", flags,
                       "location", location,
                       "header-location", header_location,
                       NULL);
}
