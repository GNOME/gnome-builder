/* ide-range.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-range"

#include "config.h"

#include "ide-location.h"
#include "ide-range.h"

typedef struct
{
  IdeLocation *begin;
  IdeLocation *end;
} IdeRangePrivate;

enum {
  PROP_0,
  PROP_BEGIN,
  PROP_END,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeRange, ide_range, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_range_set_begin (IdeRange    *self,
                     IdeLocation *location)
{
  IdeRangePrivate *priv = ide_range_get_instance_private (self);

  g_return_if_fail (IDE_IS_RANGE (self));
  g_return_if_fail (IDE_IS_LOCATION (location));

  g_set_object (&priv->begin, location);
}

static void
ide_range_set_end (IdeRange    *self,
                   IdeLocation *location)
{
  IdeRangePrivate *priv = ide_range_get_instance_private (self);

  g_return_if_fail (IDE_IS_RANGE (self));
  g_return_if_fail (IDE_IS_LOCATION (location));

  g_set_object (&priv->end, location);
}

static void
ide_range_finalize (GObject *object)
{
  IdeRange *self = (IdeRange *)object;
  IdeRangePrivate *priv = ide_range_get_instance_private (self);

  g_clear_object (&priv->begin);
  g_clear_object (&priv->end);

  G_OBJECT_CLASS (ide_range_parent_class)->finalize (object);
}

static void
ide_range_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  IdeRange *self = IDE_RANGE (object);

  switch (prop_id)
    {
    case PROP_BEGIN:
      g_value_set_object (value, ide_range_get_begin (self));
      break;

    case PROP_END:
      g_value_set_object (value, ide_range_get_end (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_range_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  IdeRange *self = IDE_RANGE (object);

  switch (prop_id)
    {
    case PROP_BEGIN:
      ide_range_set_begin (self, g_value_get_object (value));
      break;

    case PROP_END:
      ide_range_set_end (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_range_class_init (IdeRangeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_range_finalize;
  object_class->get_property = ide_range_get_property;
  object_class->set_property = ide_range_set_property;

  properties [PROP_BEGIN] =
    g_param_spec_object ("begin",
                         "Begin",
                         "The start of the range",
                         IDE_TYPE_LOCATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_END] =
    g_param_spec_object ("end",
                         "End",
                         "The end of the range",
                         IDE_TYPE_LOCATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_range_init (IdeRange *self)
{
}

IdeRange *
ide_range_new (IdeLocation *begin,
               IdeLocation *end)
{
  g_return_val_if_fail (IDE_IS_LOCATION (begin), NULL);
  g_return_val_if_fail (IDE_IS_LOCATION (end), NULL);

  return g_object_new (IDE_TYPE_RANGE,
                       "begin", begin,
                       "end", end,
                       NULL);
}

/**
 * ide_range_get_begin:
 * @self: a #IdeRange
 *
 * Returns: (transfer none): the beginning of the range
 */
IdeLocation *
ide_range_get_begin (IdeRange *self)
{
  IdeRangePrivate *priv = ide_range_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RANGE (self), NULL);

  return priv->begin;
}

/**
 * ide_range_get_end:
 * @self: a #IdeRange
 *
 * Returns: (transfer none): the end of the range
 */
IdeLocation *
ide_range_get_end (IdeRange *self)
{
  IdeRangePrivate *priv = ide_range_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RANGE (self), NULL);

  return priv->end;
}

/**
 * ide_range_to_variant:
 * @self: a #IdeRange
 *
 * Creates a variant to represent the range.
 *
 * This function will never return a floating variant.
 *
 * Returns: (transfer full): a #GVariant
 */
GVariant *
ide_range_to_variant (IdeRange *self)
{
  IdeRangePrivate *priv = ide_range_get_instance_private (self);
  GVariantDict dict;

  g_return_val_if_fail (IDE_IS_RANGE (self), NULL);

  g_variant_dict_init (&dict, NULL);

  if (priv->begin)
    {
      g_autoptr(GVariant) begin = NULL;

      if ((begin = ide_location_to_variant (priv->begin)))
        g_variant_dict_insert_value (&dict, "begin", begin);
    }

  if (priv->end)
    {
      g_autoptr(GVariant) end = NULL;

      if ((end = ide_location_to_variant (priv->end)))
        g_variant_dict_insert_value (&dict, "end", end);
    }

  return g_variant_take_ref (g_variant_dict_end (&dict));
}

/**
 * ide_range_new_from_variant:
 * @variant: a #GVariant
 *
 * Returns: (transfer full) (nullable): a new range or %NULL
 */
IdeRange *
ide_range_new_from_variant (GVariant *variant)
{
  g_autoptr(GVariant) unboxed = NULL;
  g_autoptr(GVariant) vbegin = NULL;
  g_autoptr(GVariant) vend = NULL;
  g_autoptr(IdeLocation) begin = NULL;
  g_autoptr(IdeLocation) end = NULL;
  IdeRange *self = NULL;
  GVariantDict dict;

  if (variant == NULL)
    return NULL;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
    variant = unboxed = g_variant_get_variant (variant);

  g_variant_dict_init (&dict, variant);

  if (!(vbegin = g_variant_dict_lookup_value (&dict, "begin", NULL)) ||
      !(begin = ide_location_new_from_variant (vbegin)))
    goto failure;

  if (!(vend = g_variant_dict_lookup_value (&dict, "end", NULL)) ||
      !(end = ide_location_new_from_variant (vend)))
    goto failure;

  self = ide_range_new (begin, end);

  g_variant_dict_clear (&dict);

failure:

  return self;
}
