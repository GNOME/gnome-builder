/* editor-spell-checker.c
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

#include "editor-spell-checker.h"
#include "editor-spell-language.h"
#include "editor-spell-provider.h"

struct _EditorSpellChecker
{
  GObject              parent_instance;
  EditorSpellProvider *provider;
  EditorSpellLanguage *language;
};

G_DEFINE_TYPE (EditorSpellChecker, editor_spell_checker, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_LANGUAGE,
  PROP_PROVIDER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * editor_spell_checker_new:
 *
 * Create a new #EditorSpellChecker.
 *
 * Returns: (transfer full): a newly created #EditorSpellChecker
 */
EditorSpellChecker *
editor_spell_checker_new (EditorSpellProvider *provider,
                          const char          *language)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_PROVIDER (provider) || !provider, NULL);

  if (provider == NULL)
    provider = editor_spell_provider_get_default ();

  if (language == NULL)
    language = editor_spell_provider_get_default_code (provider);

  return g_object_new (EDITOR_TYPE_SPELL_CHECKER,
                       "provider", provider,
                       "language", language,
                       NULL);
}

static void
editor_spell_checker_constructed (GObject *object)
{
  EditorSpellChecker *self = (EditorSpellChecker *)object;

  g_assert (EDITOR_IS_SPELL_CHECKER (self));

  G_OBJECT_CLASS (editor_spell_checker_parent_class)->constructed (object);

  if (self->provider == NULL)
    self->provider = editor_spell_provider_get_default ();
}

static void
editor_spell_checker_finalize (GObject *object)
{
  EditorSpellChecker *self = (EditorSpellChecker *)object;

  g_clear_object (&self->provider);
  g_clear_object (&self->language);

  G_OBJECT_CLASS (editor_spell_checker_parent_class)->finalize (object);
}

static void
editor_spell_checker_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  EditorSpellChecker *self = EDITOR_SPELL_CHECKER (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      g_value_set_object (value, editor_spell_checker_get_provider (self));
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, editor_spell_checker_get_language (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_spell_checker_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  EditorSpellChecker *self = EDITOR_SPELL_CHECKER (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      self->provider = g_value_dup_object (value);
      break;

    case PROP_LANGUAGE:
      editor_spell_checker_set_language (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_spell_checker_class_init (EditorSpellCheckerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = editor_spell_checker_constructed;
  object_class->finalize = editor_spell_checker_finalize;
  object_class->get_property = editor_spell_checker_get_property;
  object_class->set_property = editor_spell_checker_set_property;

  /**
   * EditorSpellChecker:language:
   *
   * The "language" to use when checking words with the configured
   * #EditorSpellProvider. For example, `en_US`.
   */
  properties [PROP_LANGUAGE] =
    g_param_spec_string ("language",
                         "Language",
                         "The language code",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * EditorSpellChecker:provider:
   *
   * The "provider" property contains the provider that is providing
   * information to the spell checker.
   *
   * Currently, only Enchant is supported, and requires using the
   * #EditorEnchantSpellProvider. Setting this to %NULL will get
   * the default provider.
   */
  properties [PROP_PROVIDER] =
    g_param_spec_object ("provider",
                         "Provider",
                         "The spell check provider",
                         EDITOR_TYPE_SPELL_PROVIDER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
editor_spell_checker_init (EditorSpellChecker *self)
{
}

/**
 * editor_spell_checker_get_language:
 *
 * Gets the language being used by the spell checker.
 *
 * Returns: (nullable): a string describing the current language.
 */
const char *
editor_spell_checker_get_language (EditorSpellChecker *self)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_CHECKER (self), NULL);

  return self->language ? editor_spell_language_get_code (self->language) : NULL;
}

/**
 * editor_spell_checker_set_language:
 * @self: an #EditorSpellChecker
 * @language: the language to use
 *
 * Sets the language code to use when communicating with the provider,
 * such as `en_US`.
 */
void
editor_spell_checker_set_language (EditorSpellChecker *self,
                                   const char         *language)
{
  g_return_if_fail (EDITOR_IS_SPELL_CHECKER (self));

  if (g_strcmp0 (language, editor_spell_checker_get_language (self)) != 0)
    {
      self->language = editor_spell_provider_get_language (self->provider, language);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGE]);
    }
}

/**
 * editor_spell_checker_get_provider:
 *
 * Gets the spell provider used by the spell checker.
 *
 * Currently, only Enchant-2 is supported.
 *
 * Returns: (transfer none) (not nullable): an #EditorSpellProvider
 */
EditorSpellProvider *
editor_spell_checker_get_provider (EditorSpellChecker *self)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_CHECKER (self), NULL);

  return self->provider;
}

static inline gboolean
word_is_number (const char *word,
                gssize      word_len)
{
  g_assert (word_len > 0);

  for (gssize i = 0; i < word_len; i++)
    {
      if (word[i] < '0' || word[i] > '9')
        return FALSE;
    }

  return TRUE;
}

gboolean
editor_spell_checker_check_word (EditorSpellChecker *self,
                                 const char         *word,
                                 gssize              word_len)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_CHECKER (self), FALSE);

  if (word == NULL || word_len == 0)
    return FALSE;

  if (self->language == NULL)
    return TRUE;

  if (word_len < 0)
    word_len = strlen (word);

  if (word_is_number (word, word_len))
    return TRUE;

  return editor_spell_language_contains_word (self->language, word, word_len);
}

char **
editor_spell_checker_list_corrections (EditorSpellChecker *self,
                                       const char         *word)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_CHECKER (self), NULL);
  g_return_val_if_fail (word != NULL, NULL);

  if (self->language == NULL)
    return NULL;

  return editor_spell_language_list_corrections (self->language, word, -1);
}

void
editor_spell_checker_add_word (EditorSpellChecker *self,
                               const char         *word)
{
  g_return_if_fail (EDITOR_IS_SPELL_CHECKER (self));
  g_return_if_fail (word != NULL);

  if (self->language != NULL)
    editor_spell_language_add_word (self->language, word);
}

void
editor_spell_checker_ignore_word (EditorSpellChecker *self,
                                  const char         *word)
{
  g_return_if_fail (EDITOR_IS_SPELL_CHECKER (self));
  g_return_if_fail (word != NULL);

  if (self->language != NULL)
    editor_spell_language_ignore_word (self->language, word);
}

const char *
editor_spell_checker_get_extra_word_chars (EditorSpellChecker *self)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_CHECKER (self), NULL);

  if (self->language != NULL)
    return editor_spell_language_get_extra_word_chars (self->language);

  return "";
}
