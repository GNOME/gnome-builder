/* ide-fuzzy-index-match.c
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-fuzzy-index-match"

#include "config.h"

#include "ide-fuzzy-index-match.h"

struct _IdeFuzzyIndexMatch
{
  GObject   object;
  GVariant *document;
  gchar    *key;
  gfloat    score;
  guint     priority;
};

enum {
  PROP_0,
  PROP_DOCUMENT,
  PROP_KEY,
  PROP_SCORE,
  PROP_PRIORITY,
  N_PROPS
};

G_DEFINE_TYPE (IdeFuzzyIndexMatch, ide_fuzzy_index_match, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_fuzzy_index_match_finalize (GObject *object)
{
  IdeFuzzyIndexMatch *self = (IdeFuzzyIndexMatch *)object;

  g_clear_pointer (&self->document, g_variant_unref);
  g_clear_pointer (&self->key, g_free);

  G_OBJECT_CLASS (ide_fuzzy_index_match_parent_class)->finalize (object);
}

static void
ide_fuzzy_index_match_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeFuzzyIndexMatch *self = IDE_FUZZY_INDEX_MATCH (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_variant (value, self->document);
      break;

    case PROP_SCORE:
      g_value_set_float (value, self->score);
      break;

    case PROP_KEY:
      g_value_set_string (value, self->key);
      break;

    case PROP_PRIORITY:
      g_value_set_uint (value, self->priority);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_fuzzy_index_match_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeFuzzyIndexMatch *self = IDE_FUZZY_INDEX_MATCH (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      self->document = g_value_dup_variant (value);
      break;

    case PROP_SCORE:
      self->score = g_value_get_float (value);
      break;

    case PROP_KEY:
      self->key = g_value_dup_string (value);
      break;

    case PROP_PRIORITY:
      self->priority = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ide_fuzzy_index_match_class_init (IdeFuzzyIndexMatchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_fuzzy_index_match_finalize;
  object_class->get_property = ide_fuzzy_index_match_get_property;
  object_class->set_property = ide_fuzzy_index_match_set_property;

  properties [PROP_DOCUMENT] =
    g_param_spec_variant ("document",
                          "Document",
                          "Document",
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEY] =
    g_param_spec_string ("key",
                         "Key",
                         "The string key that was inserted for the document",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_uint ("priority",
                       "Priority",
                       "The priority used when creating the index",
                       0,
                       255,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SCORE] =
    g_param_spec_float ("score",
                        "Score",
                        "Score",
                        -G_MINFLOAT,
                        G_MAXFLOAT,
                        0.0f,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_fuzzy_index_match_init (IdeFuzzyIndexMatch *match)
{
}

/**
 * ide_fuzzy_index_match_get_document:
 *
 * Returns: (transfer none): A #GVariant.
 */
GVariant *
ide_fuzzy_index_match_get_document (IdeFuzzyIndexMatch *self)
{
  g_return_val_if_fail (IDE_IS_FUZZY_INDEX_MATCH (self), NULL);

  return self->document;
}

gfloat
ide_fuzzy_index_match_get_score (IdeFuzzyIndexMatch *self)
{
  g_return_val_if_fail (IDE_IS_FUZZY_INDEX_MATCH (self), 0.0f);

  return self->score;
}

const gchar *
ide_fuzzy_index_match_get_key (IdeFuzzyIndexMatch *self)
{
  g_return_val_if_fail (IDE_IS_FUZZY_INDEX_MATCH (self), NULL);

  return self->key;
}

guint
ide_fuzzy_index_match_get_priority (IdeFuzzyIndexMatch *self)
{
  g_return_val_if_fail (IDE_IS_FUZZY_INDEX_MATCH (self), 0);

  return self->priority;
}
