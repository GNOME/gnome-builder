/* editor-spell-language.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <string.h>

#include "editor-spell-language.h"

typedef struct
{
  const char *code;
} EditorSpellLanguagePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (EditorSpellLanguage, editor_spell_language, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CODE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
editor_spell_language_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EditorSpellLanguage *self = EDITOR_SPELL_LANGUAGE (object);

  switch (prop_id)
    {
    case PROP_CODE:
      g_value_set_string (value, editor_spell_language_get_code (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_spell_language_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EditorSpellLanguage *self = EDITOR_SPELL_LANGUAGE (object);
  EditorSpellLanguagePrivate *priv = editor_spell_language_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CODE:
      priv->code = g_intern_string (g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_spell_language_class_init (EditorSpellLanguageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = editor_spell_language_get_property;
  object_class->set_property = editor_spell_language_set_property;

  properties [PROP_CODE] =
    g_param_spec_string ("code",
                         "Code",
                         "The language code",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
editor_spell_language_init (EditorSpellLanguage *self)
{
}

const char *
editor_spell_language_get_code (EditorSpellLanguage *self)
{
  EditorSpellLanguagePrivate *priv = editor_spell_language_get_instance_private (self);

  g_return_val_if_fail (EDITOR_IS_SPELL_LANGUAGE (self), NULL);

  return priv->code;
}

gboolean
editor_spell_language_contains_word (EditorSpellLanguage *self,
                                     const char          *word,
                                     gssize               word_len)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_LANGUAGE (self), FALSE);
  g_return_val_if_fail (word != NULL, FALSE);

  if (word_len < 0)
    word_len = strlen (word);

  return EDITOR_SPELL_LANGUAGE_GET_CLASS (self)->contains_word (self, word, word_len);
}

char **
editor_spell_language_list_corrections (EditorSpellLanguage *self,
                                        const char          *word,
                                        gssize               word_len)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_LANGUAGE (self), NULL);
  g_return_val_if_fail (word != NULL, NULL);
  g_return_val_if_fail (word != NULL || word_len == 0, NULL);

  if (word_len < 0)
    word_len = strlen (word);

  if (word_len == 0)
    return NULL;

  return EDITOR_SPELL_LANGUAGE_GET_CLASS (self)->list_corrections (self, word, word_len);
}

void
editor_spell_language_add_word (EditorSpellLanguage *self,
                                const char          *word)
{
  g_return_if_fail (EDITOR_IS_SPELL_LANGUAGE (self));
  g_return_if_fail (word != NULL);

  if (EDITOR_SPELL_LANGUAGE_GET_CLASS (self)->add_word)
    EDITOR_SPELL_LANGUAGE_GET_CLASS (self)->add_word (self, word);
}

void
editor_spell_language_ignore_word (EditorSpellLanguage *self,
                                   const char          *word)
{
  g_return_if_fail (EDITOR_IS_SPELL_LANGUAGE (self));
  g_return_if_fail (word != NULL);

  if (EDITOR_SPELL_LANGUAGE_GET_CLASS (self)->ignore_word)
    EDITOR_SPELL_LANGUAGE_GET_CLASS (self)->ignore_word (self, word);
}

const char *
editor_spell_language_get_extra_word_chars (EditorSpellLanguage *self)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_LANGUAGE (self), NULL);

  if (EDITOR_SPELL_LANGUAGE_GET_CLASS (self)->get_extra_word_chars)
    return EDITOR_SPELL_LANGUAGE_GET_CLASS (self)->get_extra_word_chars (self);

  return "";
}
