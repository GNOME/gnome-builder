/* ide-location.c
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

#define G_LOG_DOMAIN "ide-location"

#include "config.h"

#include "ide-location.h"

typedef struct
{
  GFile *file;
  gint   line;
  gint   line_offset;
  gint   offset;
} IdeLocationPrivate;

enum {
  PROP_0,
  PROP_FILE,
  PROP_LINE,
  PROP_LINE_OFFSET,
  PROP_OFFSET,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeLocation, ide_location, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_location_set_file (IdeLocation *self,
                       GFile       *file)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_assert (IDE_IS_LOCATION (self));

  g_set_object (&priv->file, file);
}

static void
ide_location_set_line (IdeLocation *self,
                       gint         line)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_assert (IDE_IS_LOCATION (self));

  priv->line = CLAMP (line, -1, G_MAXINT);
}

static void
ide_location_set_line_offset (IdeLocation *self,
                              gint         line_offset)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_assert (IDE_IS_LOCATION (self));

  priv->line_offset = CLAMP (line_offset, -1, G_MAXINT);
}

static void
ide_location_set_offset (IdeLocation *self,
                         gint         offset)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_assert (IDE_IS_LOCATION (self));

  priv->offset = CLAMP (offset, -1, G_MAXINT);
}

static void
ide_location_dispose (GObject *object)
{
  IdeLocation *self = (IdeLocation *)object;
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_clear_object (&priv->file);

  G_OBJECT_CLASS (ide_location_parent_class)->dispose (object);
}

static void
ide_location_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeLocation *self = IDE_LOCATION (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_location_get_file (self));
      break;

    case PROP_LINE:
      g_value_set_int (value, ide_location_get_line (self));
      break;

    case PROP_LINE_OFFSET:
      g_value_set_int (value, ide_location_get_line_offset (self));
      break;

    case PROP_OFFSET:
      g_value_set_int (value, ide_location_get_offset (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, ide_location_dup_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_location_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeLocation *self = IDE_LOCATION (object);

  switch (prop_id)
    {
    case PROP_FILE:
      ide_location_set_file (self, g_value_get_object (value));
      break;

    case PROP_LINE:
      ide_location_set_line (self, g_value_get_int (value));
      break;

    case PROP_LINE_OFFSET:
      ide_location_set_line_offset (self, g_value_get_int (value));
      break;

    case PROP_OFFSET:
      ide_location_set_offset (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_location_class_init (IdeLocationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_location_dispose;
  object_class->get_property = ide_location_get_property;
  object_class->set_property = ide_location_set_property;

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the location",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The file representing the location",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LINE] =
    g_param_spec_int ("line",
                      "Line",
                      "The line number within the file, starting from 0 or -1 for unknown",
                      -1, G_MAXINT, -1,
                      (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LINE_OFFSET] =
    g_param_spec_int ("line-offset",
                      "Line Offset",
                      "The offset within the line, starting from 0 or -1 for unknown",
                      -1, G_MAXINT, -1,
                      (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_OFFSET] =
    g_param_spec_int ("offset",
                      "Offset",
                      "The offset within the file in characters, or -1 if unknown",
                      -1, G_MAXINT, -1,
                      (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_location_init (IdeLocation *self)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  priv->line = -1;
  priv->line_offset = -1;
  priv->offset = -1;
}

/**
 * ide_location_get_file:
 * @self: a #IdeLocation
 *
 * Gets the file within the location.
 *
 * Returns: (transfer none) (nullable): a #GFile or %NULL
 */
GFile *
ide_location_get_file (IdeLocation *self)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LOCATION (self), NULL);

  return priv->file;
}

/**
 * ide_location_get_line:
 * @self: a #IdeLocation
 *
 * Gets the line within the #IdeLocation:file, or -1 if it is unknown.
 *
 * Returns: the line number, or -1.
 */
gint
ide_location_get_line (IdeLocation *self)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LOCATION (self), -1);

  return priv->line;
}

/**
 * ide_location_get_line_offset:
 * @self: a #IdeLocation
 *
 * Gets the offset within the #IdeLocation:line, or -1 if it is unknown.
 *
 * Returns: the line offset, or -1.
 */
gint
ide_location_get_line_offset (IdeLocation *self)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LOCATION (self), -1);

  return priv->line_offset;
}

/**
 * ide_location_get_offset:
 * @self: a #IdeLocation
 *
 * Gets the offset within the file in characters, or -1 if it is unknown.
 *
 * Returns: the line offset, or -1.
 */
gint
ide_location_get_offset (IdeLocation *self)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LOCATION (self), -1);

  return priv->offset;
}

/**
 * ide_location_dup:
 * @self: a #IdeLocation
 *
 * Makes a deep copy of @self.
 *
 * Returns: (transfer full): a new #IdeLocation
 */
IdeLocation *
ide_location_dup (IdeLocation *self)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_return_val_if_fail (!self || IDE_IS_LOCATION (self), NULL);

  if (self == NULL)
    return NULL;

  return g_object_new (IDE_TYPE_LOCATION,
                       "file", priv->file,
                       "line", priv->line,
                       "line-offset", priv->line_offset,
                       "offset", priv->offset,
                       NULL);
}

/**
 * ide_location_to_variant:
 * @self: a #IdeLocation
 *
 * Serializes the location into a variant that can be used to transport
 * across IPC boundaries.
 *
 * This function will never return a variant with a floating reference.
 *
 * Returns: (transfer full): a #GVariant
 */
GVariant *
ide_location_to_variant (IdeLocation *self)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);
  g_autofree gchar *uri = NULL;
  GVariantDict dict;

  g_return_val_if_fail (self != NULL, NULL);

  g_variant_dict_init (&dict, NULL);

  uri = g_file_get_uri (priv->file);

  g_variant_dict_insert (&dict, "uri", "s", uri);
  g_variant_dict_insert (&dict, "line", "i", priv->line);
  g_variant_dict_insert (&dict, "line-offset", "i", priv->line_offset);

  return g_variant_take_ref (g_variant_dict_end (&dict));
}

IdeLocation *
ide_location_new (GFile *file,
                  gint   line,
                  gint   line_offset)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  line = CLAMP (line, -1, G_MAXINT);
  line_offset = CLAMP (line_offset, -1, G_MAXINT);

  return g_object_new (IDE_TYPE_LOCATION,
                       "file", file,
                       "line", line,
                       "line-offset", line_offset,
                       NULL);
}

/**
 * ide_location_new_with_offset:
 * @file: a #GFile
 * @line: a line number starting from 0, or -1 if unknown
 * @line_offset: a line offset starting from 0, or -1 if unknown
 * @offset: a charcter offset in file starting from 0, or -1 if unknown
 *
 * Returns: (transfer full): an #IdeLocation
 */
IdeLocation *
ide_location_new_with_offset (GFile *file,
                              gint   line,
                              gint   line_offset,
                              gint   offset)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  line = CLAMP (line, -1, G_MAXINT);
  line_offset = CLAMP (line_offset, -1, G_MAXINT);
  offset = CLAMP (offset, -1, G_MAXINT);

  return g_object_new (IDE_TYPE_LOCATION,
                       "file", file,
                       "line", line,
                       "line-offset", line_offset,
                       "offset", offset,
                       NULL);
}

/**
 * ide_location_new_from_variant:
 * @variant: (nullable): a #GVariant or %NULL
 *
 * Creates a new #IdeLocation using the serialized form from a
 * previously serialized #GVariant.
 *
 * As a convenience, if @variant is %NULL, %NULL is returned.
 *
 * See also: ide_location_to_variant()
 *
 * Returns: (transfer full) (nullable): a #GVariant if succesful;
 *   otherwise %NULL.
 */
IdeLocation *
ide_location_new_from_variant (GVariant *variant)
{
  g_autoptr(GVariant) unboxed = NULL;
  g_autoptr(GFile) file = NULL;
  IdeLocation *self = NULL;
  GVariantDict dict;
  const gchar *uri;
  guint32 line;
  guint32 line_offset;

  if (variant == NULL)
    return NULL;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
    variant = unboxed = g_variant_get_variant (variant);

  g_variant_dict_init (&dict, variant);

  if (!g_variant_dict_lookup (&dict, "uri", "&s", &uri))
    goto failure;

  if (!g_variant_dict_lookup (&dict, "line", "i", &line))
    line = 0;

  if (!g_variant_dict_lookup (&dict, "line-offset", "i", &line_offset))
    line_offset = 0;

  file = g_file_new_for_uri (uri);

  self = ide_location_new (file, line, line_offset);

failure:
  g_variant_dict_clear (&dict);

  return self;
}

static gint
file_compare (GFile *a,
              GFile *b)
{
  g_autofree gchar *uri_a = g_file_get_uri (a);
  g_autofree gchar *uri_b = g_file_get_uri (b);

  return g_strcmp0 (uri_a, uri_b);
}

gboolean
ide_location_compare (IdeLocation *a,
                      IdeLocation *b)
{
  IdeLocationPrivate *priv_a = ide_location_get_instance_private (a);
  IdeLocationPrivate *priv_b = ide_location_get_instance_private (b);
  gint ret;

  g_assert (IDE_IS_LOCATION (a));
  g_assert (IDE_IS_LOCATION (b));

  if (priv_a->file && priv_b->file)
    {
      if (0 != (ret = file_compare (priv_a->file, priv_b->file)))
        return ret;
    }
  else if (priv_a->file)
    return -1;
  else if (priv_b->file)
    return 1;

  if (0 != (ret = priv_a->line - priv_b->line))
    return ret;

  return priv_a->line_offset - priv_b->line_offset;
}

guint
ide_location_hash (IdeLocation *self)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LOCATION (self), 0);

  return g_file_hash (priv->file) ^ g_int_hash (&priv->line) ^ g_int_hash (&priv->line_offset);
}

gboolean
ide_location_equal (IdeLocation *a,
                    IdeLocation *b)
{
  IdeLocationPrivate *a_priv = ide_location_get_instance_private (a);
  IdeLocationPrivate *b_priv = ide_location_get_instance_private (b);

  g_return_val_if_fail (!a || IDE_IS_LOCATION (a), FALSE);
  g_return_val_if_fail (!b || IDE_IS_LOCATION (b), FALSE);

  if (a == NULL || b == NULL)
    return FALSE;

  if (a_priv->file == NULL || b_priv->file == NULL)
    return FALSE;

  if (G_OBJECT_TYPE (a) != G_OBJECT_TYPE (b))
    return FALSE;

  if (!g_file_equal (a_priv->file, b_priv->file))
    return FALSE;

  return a_priv->line == b_priv->line &&
         a_priv->line_offset == b_priv->line_offset &&
         a_priv->offset == b_priv->offset;
}

/**
 * ide_location_dup_title:
 * @self: a #IdeLocation
 *
 * Gets a title string for the location, usually in the form of
 *   shortname:line:column
 *
 * Returns: (transfer full) (nullable): A new string containing the
 *   something suitable to be used as a title for diagnostics.
 */
char *
ide_location_dup_title (IdeLocation *self)
{
  IdeLocationPrivate *priv = ide_location_get_instance_private (self);
  g_autofree char *name = NULL;

  g_return_val_if_fail (IDE_IS_LOCATION (self), NULL);

  if (priv->file == NULL)
    return NULL;

  if (!(name = g_file_get_basename (priv->file)))
    return NULL;

  if (priv->line >= 0 && priv->line_offset >= 0)
    return g_strdup_printf ("%s:%d:%d", name, priv->line, priv->line_offset);

  if (priv->line >= 0)
    return g_strdup_printf ("%s:%d", name, priv->line);

  return g_steal_pointer (&name);
}
