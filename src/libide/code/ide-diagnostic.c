/* ide-diagnostic.c
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

#define G_LOG_DOMAIN "ide-diagnostic"

#include "config.h"

#include "ide-code-enums.h"
#include "ide-diagnostic.h"
#include "ide-location.h"
#include "ide-range.h"
#include "ide-text-edit.h"

typedef struct
{
  IdeDiagnosticSeverity  severity;
  guint                  hash;
  gchar                 *text;
  IdeLocation           *location;
  GPtrArray             *ranges;
  GPtrArray             *fixits;
  IdeMarkedKind          marked_kind;
} IdeDiagnosticPrivate;

enum {
  PROP_0,
  PROP_DISPLAY_TEXT,
  PROP_LOCATION,
  PROP_SEVERITY,
  PROP_TEXT,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeDiagnostic, ide_diagnostic, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_diagnostic_set_location (IdeDiagnostic *self,
                             IdeLocation   *location)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC (self));

  g_set_object (&priv->location, location);
}

static void
ide_diagnostic_set_text (IdeDiagnostic *self,
                         const gchar   *text)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC (self));

  priv->text = g_strdup (text);
}

static void
ide_diagnostic_set_severity (IdeDiagnostic         *self,
                             IdeDiagnosticSeverity  severity)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC (self));

  priv->severity = severity;
}

static void
ide_diagnostic_finalize (GObject *object)
{
  IdeDiagnostic *self = (IdeDiagnostic *)object;
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_clear_pointer (&priv->text, g_free);
  g_clear_pointer (&priv->ranges, g_ptr_array_unref);
  g_clear_pointer (&priv->fixits, g_ptr_array_unref);
  g_clear_object (&priv->location);

  G_OBJECT_CLASS (ide_diagnostic_parent_class)->finalize (object);
}

static void
ide_diagnostic_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeDiagnostic *self = IDE_DIAGNOSTIC (object);

  switch (prop_id)
    {
    case PROP_LOCATION:
      g_value_set_object (value, ide_diagnostic_get_location (self));
      break;

    case PROP_SEVERITY:
      g_value_set_enum (value, ide_diagnostic_get_severity (self));
      break;

    case PROP_DISPLAY_TEXT:
      g_value_take_string (value, ide_diagnostic_get_text_for_display (self));
      break;

    case PROP_TEXT:
      g_value_set_string (value, ide_diagnostic_get_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_diagnostic_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeDiagnostic *self = IDE_DIAGNOSTIC (object);

  switch (prop_id)
    {
    case PROP_LOCATION:
      ide_diagnostic_set_location (self, g_value_get_object (value));
      break;

    case PROP_SEVERITY:
      ide_diagnostic_set_severity (self, g_value_get_enum (value));
      break;

    case PROP_TEXT:
      ide_diagnostic_set_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_diagnostic_class_init (IdeDiagnosticClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_diagnostic_finalize;
  object_class->get_property = ide_diagnostic_get_property;
  object_class->set_property = ide_diagnostic_set_property;

  properties [PROP_LOCATION] =
    g_param_spec_object ("location",
                         "Location",
                         "The location of the diagnostic",
                         IDE_TYPE_LOCATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEVERITY] =
    g_param_spec_enum ("severity",
                       "Severity",
                       "The severity of the diagnostic",
                       IDE_TYPE_DIAGNOSTIC_SEVERITY,
                       IDE_DIAGNOSTIC_IGNORED,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TEXT] =
    g_param_spec_string ("text",
                         "Text",
                         "The text of the diagnostic",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISPLAY_TEXT] =
    g_param_spec_string ("display-text",
                         "Display Text",
                         "The text formatted for display",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_diagnostic_init (IdeDiagnostic *self)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  priv->marked_kind = IDE_MARKED_KIND_PLAINTEXT;
}

/**
 * ide_diagnostic_get_location:
 * @self: a #IdeDiagnostic
 *
 * Gets the location of the diagnostic.
 *
 * See also: ide_diagnostic_get_range().
 *
 * Returns: (transfer none) (nullable): an #IdeLocation or %NULL
 */
IdeLocation *
ide_diagnostic_get_location (IdeDiagnostic *self)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), NULL);

  if (priv->location != NULL)
    return priv->location;

  if (priv->ranges != NULL && priv->ranges->len > 0)
    {
      IdeRange *range = g_ptr_array_index (priv->ranges, 0);
      return ide_range_get_begin (range);
    }

  return NULL;
}

/**
 * ide_diagnostic_get_file:
 * @self: a #IdeDiagnostic
 *
 * Gets the file containing the diagnostic, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeLocation or %NULL
 */
GFile *
ide_diagnostic_get_file (IdeDiagnostic *self)
{
  IdeLocation *location;

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), NULL);

  if ((location = ide_diagnostic_get_location (self)))
    return ide_location_get_file (location);

  return NULL;
}

/**
 * ide_diagnostic_get_text_for_display:
 * @self: an #IdeDiagnostic
 *
 * This creates a new string that is formatted using the diagnostics
 * line number, column, severity, and message text in the format
 * "line:column: severity: message".
 *
 * This can be convenient when wanting to quickly display a
 * diagnostic such as in a tooltip.
 *
 * Returns: (transfer full): string containing the text formatted for
 *   display.
 */
gchar *
ide_diagnostic_get_text_for_display (IdeDiagnostic *self)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);
  IdeLocation *location;
  const gchar *severity;
  guint line = 0;
  guint column = 0;

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), NULL);

  severity = ide_diagnostic_severity_to_string (priv->severity);
  location = ide_diagnostic_get_location (self);

  if (location != NULL)
    {
      line = ide_location_get_line (location) + 1;
      column = ide_location_get_line_offset (location) + 1;
    }

  return g_strdup_printf ("%u:%u: %s: %s", line, column, severity, priv->text);
}

/**
 * ide_diagnostic_severity_to_string:
 * @severity: a #IdeDiagnosticSeverity
 *
 * Returns a string suitable to represent the diagnsotic severity.
 *
 * Returns: a string
 */
const gchar *
ide_diagnostic_severity_to_string (IdeDiagnosticSeverity severity)
{
  switch (severity)
    {
    case IDE_DIAGNOSTIC_IGNORED:
      return "ignored";

    case IDE_DIAGNOSTIC_NOTE:
      return "note";

    case IDE_DIAGNOSTIC_UNUSED:
      return "unused";

    case IDE_DIAGNOSTIC_DEPRECATED:
      return "deprecated";

    case IDE_DIAGNOSTIC_WARNING:
      return "warning";

    case IDE_DIAGNOSTIC_ERROR:
      return "error";

    case IDE_DIAGNOSTIC_FATAL:
      return "fatal";

    default:
      return "unknown";
    }
}

guint
ide_diagnostic_get_n_ranges (IdeDiagnostic *self)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), 0);

  return priv->ranges ? priv->ranges->len : 0;
}

/**
 * ide_diagnostic_get_range:
 *
 * Retrieves the range found at @index. It is a programming error to call this
 * function with a value greater or equal to ide_diagnostic_get_n_ranges().
 *
 * Returns: (transfer none) (nullable): An #IdeRange
 */
IdeRange *
ide_diagnostic_get_range (IdeDiagnostic *self,
                          guint          index)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), NULL);

  if (priv->ranges)
    {
      if (index < priv->ranges->len)
        return g_ptr_array_index (priv->ranges, index);
    }

  return NULL;
}

guint
ide_diagnostic_get_n_fixits (IdeDiagnostic *self)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), 0);

  return (priv->fixits != NULL) ? priv->fixits->len : 0;
}

/**
 * ide_diagnostic_get_fixit:
 * @self: an #IdeDiagnostic.
 * @index: The index of the fixit.
 *
 * Gets the fixit denoted by @index. This value should be less than the value
 * returned from ide_diagnostic_get_n_fixits().
 *
 * Returns: (transfer none) (nullable): An #IdeTextEdit
 */
IdeTextEdit *
ide_diagnostic_get_fixit (IdeDiagnostic *self,
                          guint          index)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), NULL);
  g_return_val_if_fail (priv->fixits, NULL);

  if (priv->fixits != NULL)
    {
      if (index < priv->fixits->len)
        return g_ptr_array_index (priv->fixits, index);
    }

  return NULL;
}

gint
ide_diagnostic_compare (IdeDiagnostic *a,
                        IdeDiagnostic *b)
{
  IdeDiagnosticPrivate *priv_a = ide_diagnostic_get_instance_private (a);
  IdeDiagnosticPrivate *priv_b = ide_diagnostic_get_instance_private (b);
  gint ret;

  g_assert (IDE_IS_DIAGNOSTIC (a));
  g_assert (IDE_IS_DIAGNOSTIC (b));

  /* Severity is 0..N where N is more important. So reverse comparator. */
  if (0 != (ret = (gint)priv_b->severity - (gint)priv_a->severity))
    return ret;

  if (priv_a->location && priv_b->location)
    {
      if (0 != (ret = ide_location_compare (priv_a->location, priv_b->location)))
        return ret;
    }

  return g_strcmp0 (priv_a->text, priv_b->text);
}

const gchar *
ide_diagnostic_get_text (IdeDiagnostic *self)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), NULL);

  return priv->text;
}

IdeDiagnosticSeverity
ide_diagnostic_get_severity (IdeDiagnostic *self)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), 0);

  return priv->severity;
}

/**
 * ide_diagnostic_add_range:
 * @self: a #IdeDiagnostic
 * @range: an #IdeRange
 *
 * Adds a source range to the diagnostic.
 */
void
ide_diagnostic_add_range (IdeDiagnostic *self,
                          IdeRange      *range)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC (self));
  g_return_if_fail (IDE_IS_RANGE (range));

  if (priv->ranges == NULL)
    priv->ranges = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (priv->ranges, g_object_ref (range));
}

/**
 * ide_diagnostic_take_range:
 * @self: a #IdeDiagnostic
 * @range: (transfer full): an #IdeRange
 *
 * Adds a source range to the diagnostic, but does not increment the
 * reference count of @range.
 */
void
ide_diagnostic_take_range (IdeDiagnostic *self,
                           IdeRange      *range)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC (self));
  g_return_if_fail (IDE_IS_RANGE (range));

  if (priv->ranges == NULL)
    priv->ranges = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (priv->ranges, g_steal_pointer (&range));
}

/**
 * ide_diagnostic_add_fixit:
 * @self: a #IdeDiagnostic
 * @fixit: an #IdeTextEdit
 *
 * Adds a source fixit to the diagnostic.
 */
void
ide_diagnostic_add_fixit (IdeDiagnostic *self,
                          IdeTextEdit   *fixit)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC (self));
  g_return_if_fail (IDE_IS_TEXT_EDIT (fixit));

  if (priv->fixits == NULL)
    priv->fixits = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (priv->fixits, g_object_ref (fixit));
}

/**
 * ide_diagnostic_take_fixit:
 * @self: a #IdeDiagnostic
 * @fixit: (transfer full): an #IdeTextEdit
 *
 * Adds a source fixit to the diagnostic, but does not increment the
 * reference count of @fixit.
 */
void
ide_diagnostic_take_fixit (IdeDiagnostic *self,
                           IdeTextEdit   *fixit)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC (self));
  g_return_if_fail (IDE_IS_TEXT_EDIT (fixit));

  if (priv->fixits == NULL)
    priv->fixits = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (priv->fixits, g_steal_pointer (&fixit));
}

IdeDiagnostic *
ide_diagnostic_new (IdeDiagnosticSeverity  severity,
                    const gchar           *message,
                    IdeLocation           *location)
{
  return g_object_new (IDE_TYPE_DIAGNOSTIC,
                       "severity", severity,
                       "location", location,
                       "text", message,
                       NULL);
}

guint
ide_diagnostic_hash (IdeDiagnostic *self)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), 0);

  if (priv->hash == 0)
    {
      guint hash = g_str_hash (priv->text ?: "");
      if (priv->location)
        hash ^= ide_location_hash (priv->location);
      if (priv->fixits)
        hash ^= g_int_hash (&priv->fixits->len);
      if (priv->ranges)
        hash ^= g_int_hash (&priv->ranges->len);
      priv->hash = hash;
    }

  return priv->hash;
}

gboolean
ide_diagnostic_equal (IdeDiagnostic *a,
                      IdeDiagnostic *b)
{
  IdeDiagnosticPrivate *a_priv = ide_diagnostic_get_instance_private (a);
  IdeDiagnosticPrivate *b_priv = ide_diagnostic_get_instance_private (b);

  g_return_val_if_fail (!a || IDE_IS_DIAGNOSTIC (a), FALSE);
  g_return_val_if_fail (!b || IDE_IS_DIAGNOSTIC (b), FALSE);

  if (a == NULL || b == NULL)
    return FALSE;

  if (G_OBJECT_TYPE (a) != G_OBJECT_TYPE (b))
    return FALSE;

  if (ide_diagnostic_hash (a) != ide_diagnostic_hash (b))
    return FALSE;

  if (g_strcmp0 (a_priv->text, b_priv->text) != 0)
    return FALSE;

  if (!ide_location_equal (a_priv->location, b_priv->location))
    return FALSE;

  return TRUE;
}

/**
 * ide_diagnostic_to_variant:
 * @self: a #IdeDiagnostic
 *
 * Creates a #GVariant to represent the diagnostic. This can be useful when
 * working in subprocesses to serialize the diagnostic.
 *
 * This function will never return a floating variant.
 *
 * Returns: (transfer full): a #GVariant
 */
GVariant *
ide_diagnostic_to_variant (IdeDiagnostic *self)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);
  GVariantDict dict;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), NULL);

  g_variant_dict_init (&dict, NULL);

  g_variant_dict_insert (&dict, "text", "s", priv->text ?: "");
  g_variant_dict_insert (&dict, "severity", "u", priv->severity);

  if (priv->location != NULL)
    {
      g_autoptr(GVariant) vloc = ide_location_to_variant (priv->location);

      if (vloc != NULL)
        g_variant_dict_insert_value (&dict, "location", vloc);
    }

  if (priv->ranges != NULL && priv->ranges->len > 0)
    {
      GVariantBuilder builder;

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

      for (guint i = 0; i < priv->ranges->len; i++)
        {
          IdeRange *range = g_ptr_array_index (priv->ranges, i);
          g_autoptr(GVariant) vrange = ide_range_to_variant (range);

          g_variant_builder_add_value (&builder, vrange);
        }

      g_variant_dict_insert_value (&dict, "ranges", g_variant_builder_end (&builder));
    }

  if (priv->fixits != NULL && priv->fixits->len > 0)
    {
      GVariantBuilder builder;

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

      for (guint i = 0; i < priv->fixits->len; i++)
        {
          IdeTextEdit *fixit = g_ptr_array_index (priv->fixits, i);
          g_autoptr(GVariant) vfixit = ide_text_edit_to_variant (fixit);

          g_variant_builder_add_value (&builder, vfixit);
        }

      g_variant_dict_insert_value (&dict, "fixits", g_variant_builder_end (&builder));
    }

  return g_variant_take_ref (g_variant_dict_end (&dict));
}

/**
 * ide_diagnostic_new_from_variant:
 * @variant: (nullable): a #GVariant or %NULL
 *
 * Creates a new #GVariant using the data contained in @variant.
 *
 * If @variant is %NULL or Upon failure, %NULL is returned.
 *
 * Returns: (nullable) (transfer full): a #IdeDiagnostic or %NULL
 */
IdeDiagnostic *
ide_diagnostic_new_from_variant (GVariant *variant)
{
  g_autoptr(IdeLocation) loc = NULL;
  g_autoptr(GVariant) vloc = NULL;
  g_autoptr(GVariant) unboxed = NULL;
  g_autoptr(GVariant) ranges = NULL;
  g_autoptr(GVariant) fixits = NULL;
  IdeDiagnostic *self;
  GVariantDict dict;
  GVariantIter iter;
  const gchar *text;
  guint32 severity;

  if (variant == NULL)
    return NULL;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
    variant = unboxed = g_variant_get_variant (variant);

  if (!g_variant_is_of_type (variant, G_VARIANT_TYPE_VARDICT))
    return NULL;

  g_variant_dict_init (&dict, variant);

  if (!g_variant_dict_lookup (&dict, "text", "&s", &text))
    text = NULL;

  if (!g_variant_dict_lookup (&dict, "severity", "u", &severity))
    severity = 0;

  if ((vloc = g_variant_dict_lookup_value (&dict, "location", NULL)))
    loc = ide_location_new_from_variant (vloc);

  if (!(self = ide_diagnostic_new (severity, text, loc)))
    goto failure;

  /* Ranges */
  if ((ranges = g_variant_dict_lookup_value (&dict, "ranges", NULL)))
    {
      GVariant *vrange;

      g_variant_iter_init (&iter, ranges);

      while ((vrange = g_variant_iter_next_value (&iter)))
        {
          IdeRange *range;

          if ((range = ide_range_new_from_variant (vrange)))
            ide_diagnostic_take_range (self, g_steal_pointer (&range));

          g_variant_unref (vrange);
        }
    }

  /* Fixits */
  if ((fixits = g_variant_dict_lookup_value (&dict, "fixits", NULL)))
    {
      GVariant *vfixit;

      g_variant_iter_init (&iter, fixits);

      while ((vfixit = g_variant_iter_next_value (&iter)))
        {
          IdeTextEdit *fixit;

          if ((fixit = ide_text_edit_new_from_variant (vfixit)))
            ide_diagnostic_take_fixit (self, g_steal_pointer (&fixit));

          g_variant_unref (vfixit);
        }
    }

failure:

  g_variant_dict_clear (&dict);

  return self;
}

IdeMarkedKind
ide_diagnostic_get_marked_kind (IdeDiagnostic *self)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIAGNOSTIC (self), IDE_MARKED_KIND_PLAINTEXT);

  return priv->marked_kind;
}

void
ide_diagnostic_set_marked_kind (IdeDiagnostic *self,
                                IdeMarkedKind  marked_kind)
{
  IdeDiagnosticPrivate *priv = ide_diagnostic_get_instance_private (self);

  g_return_if_fail (IDE_IS_DIAGNOSTIC (self));

  priv->marked_kind = marked_kind;
}
