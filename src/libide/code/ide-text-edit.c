/* ide-text-edit.c
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

#define G_LOG_DOMAIN "ide-text-edit"

#include "config.h"

#include "ide-buffer.h"
#include "ide-text-edit.h"
#include "ide-text-edit-private.h"
#include "ide-location.h"
#include "ide-range.h"

typedef struct
{
  IdeRange       *range;
  gchar          *text;

  /* No references, cleared in apply */
  GtkTextMark    *begin_mark;
  GtkTextMark    *end_mark;
} IdeTextEditPrivate;

enum {
  PROP_0,
  PROP_RANGE,
  PROP_TEXT,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeTextEdit, ide_text_edit, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_text_edit_finalize (GObject *object)
{
  IdeTextEdit *self = (IdeTextEdit *)object;
  IdeTextEditPrivate *priv = ide_text_edit_get_instance_private (self);

  g_clear_object (&priv->range);
  g_clear_pointer (&priv->text, g_free);

  G_OBJECT_CLASS (ide_text_edit_parent_class)->finalize (object);
}

static void
ide_text_edit_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeTextEdit *self = IDE_TEXT_EDIT (object);

  switch (prop_id)
    {
    case PROP_RANGE:
      g_value_set_object (value, ide_text_edit_get_range (self));
      break;

    case PROP_TEXT:
      g_value_set_string (value, ide_text_edit_get_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_text_edit_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeTextEdit *self = IDE_TEXT_EDIT (object);

  switch (prop_id)
    {
    case PROP_RANGE:
      ide_text_edit_set_range (self, g_value_get_object (value));
      break;

    case PROP_TEXT:
      ide_text_edit_set_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_text_edit_class_init (IdeTextEditClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_text_edit_finalize;
  object_class->get_property = ide_text_edit_get_property;
  object_class->set_property = ide_text_edit_set_property;

  properties [PROP_RANGE] =
    g_param_spec_object ("range",
                         "Range",
                         "The range for the text edit",
                         IDE_TYPE_RANGE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TEXT] =
    g_param_spec_string ("text",
                         "Text",
                         "The text to replace",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_text_edit_init (IdeTextEdit *self)
{
}

/**
 * ide_text_edit_get_text:
 * @self: a #IdeTextEdit
 *
 * Gets the text for the edit.
 *
 * Returns: (nullable): the text to replace, or %NULL
 */
const gchar *
ide_text_edit_get_text (IdeTextEdit *self)
{
  IdeTextEditPrivate *priv = ide_text_edit_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEXT_EDIT (self), NULL);

  return priv->text;
}

/**
 * ide_text_edit_get_range:
 * @self: a #IdeTextEdit
 *
 * Gets the range for the edit.
 *
 * Returns: (transfer none) (nullable): the range for the replacement, or %NULL
 */
IdeRange *
ide_text_edit_get_range (IdeTextEdit *self)
{
  IdeTextEditPrivate *priv = ide_text_edit_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEXT_EDIT (self), NULL);

  return priv->range;
}

void
_ide_text_edit_apply (IdeTextEdit *self,
                      IdeBuffer   *buffer)
{
  IdeTextEditPrivate *priv = ide_text_edit_get_instance_private (self);
  g_autofree char *title = NULL;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_TEXT_EDIT (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &begin, priv->begin_mark);
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &end, priv->end_mark);

  title = ide_buffer_dup_title (buffer);

  g_debug ("Applying edit in %s at %u:%u replacing with %u characters",
           title,
           gtk_text_iter_get_offset (&begin),
           gtk_text_iter_get_offset (&end),
           (guint) (priv->text ? g_utf8_strlen (priv->text, -1) : 0));

  gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);

  /* Refetch insert mark incase signal handlers modified things */
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &begin, priv->begin_mark);
  gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, priv->text, -1);

  gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), priv->begin_mark);
  gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), priv->end_mark);
}

void
_ide_text_edit_prepare (IdeTextEdit *self,
                        IdeBuffer   *buffer)
{
  IdeTextEditPrivate *priv = ide_text_edit_get_instance_private (self);
  IdeLocation *begin;
  IdeLocation *end;
  GtkTextIter begin_iter;
  GtkTextIter end_iter;

  g_assert (IDE_IS_TEXT_EDIT (self));
  g_assert (IDE_IS_BUFFER (buffer));

  begin = ide_range_get_begin (priv->range);
  end = ide_range_get_end (priv->range);

  ide_buffer_get_iter_at_location (buffer, &begin_iter, begin);
  priv->begin_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer),
                                                  NULL,
                                                  &begin_iter,
                                                  TRUE);

  ide_buffer_get_iter_at_location (buffer, &end_iter, end);
  priv->end_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer),
                                                NULL,
                                                &end_iter,
                                                FALSE);
}

IdeTextEdit *
ide_text_edit_new (IdeRange    *range,
                   const gchar *text)
{
  g_return_val_if_fail (IDE_IS_RANGE (range), NULL);

  return g_object_new (IDE_TYPE_TEXT_EDIT,
                       "range", range,
                       "text", text,
                       NULL);
}

/**
 * ide_text_edit_to_variant:
 * @self: a #IdeTextEdit
 *
 * Creates a #GVariant to represent a text_edit.
 *
 * This function will never return a floating variant.
 *
 * Returns: (transfer full): a #GVariant
 */
GVariant *
ide_text_edit_to_variant (IdeTextEdit *self)
{
  IdeTextEditPrivate *priv = ide_text_edit_get_instance_private (self);
  GVariantDict dict;
  g_autoptr(GVariant) vrange = NULL;

  g_return_val_if_fail (self != NULL, NULL);

  g_variant_dict_init (&dict, NULL);

  g_variant_dict_insert (&dict, "text", "s", priv->text ?: "");

  if ((vrange = ide_range_to_variant (priv->range)))
    g_variant_dict_insert_value (&dict, "range", vrange);

  return g_variant_take_ref (g_variant_dict_end (&dict));
}

/**
 * ide_text_edit_new_from_variant:
 * @variant: (nullable): a #GVariant
 *
 * Creates a new #IdeTextEdit from the variant.
 *
 * If @variant is %NULL, %NULL is returned.
 *
 * Returns: (transfer full) (nullable): an #IdeTextEdit or %NULL
 */
IdeTextEdit *
ide_text_edit_new_from_variant (GVariant *variant)
{
  g_autoptr(GVariant) unboxed = NULL;
  g_autoptr(GVariant) vrange = NULL;
  g_autoptr(IdeRange) range = NULL;
  GVariantDict dict;
  const gchar *text;
  IdeTextEdit *self = NULL;

  if (variant == NULL)
    return NULL;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
    variant = unboxed = g_variant_get_variant (variant);

  g_variant_dict_init (&dict, variant);

  if (!g_variant_dict_lookup (&dict, "text", "&s", &text))
    text = "";

  if ((vrange = g_variant_dict_lookup_value (&dict, "range", NULL)))
    {
      if (!(range = ide_range_new_from_variant (vrange)))
        goto failed;
    }

  self = ide_text_edit_new (range, text);

failed:

  g_variant_dict_clear (&dict);

  return self;
}

void
ide_text_edit_set_text (IdeTextEdit *self,
                        const gchar *text)
{
  IdeTextEditPrivate *priv = ide_text_edit_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEXT_EDIT (self));

  if (g_set_str (&priv->text, text))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TEXT]);
}

void
ide_text_edit_set_range (IdeTextEdit *self,
                         IdeRange    *range)
{
  IdeTextEditPrivate *priv = ide_text_edit_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEXT_EDIT (self));

  if (g_set_object (&priv->range, range))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RANGE]);
}
