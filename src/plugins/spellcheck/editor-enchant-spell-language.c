/* editor-enchant-spell-language.c
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

#include <enchant.h>

#include "editor-enchant-spell-language.h"

struct _EditorEnchantSpellLanguage
{
  EditorSpellLanguage parent_instance;
  PangoLanguage *language;
  EnchantDict *native;
  char *extra_word_chars;
};

G_DEFINE_TYPE (EditorEnchantSpellLanguage, editor_enchant_spell_language, EDITOR_TYPE_SPELL_LANGUAGE)

enum {
  PROP_0,
  PROP_NATIVE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * editor_enchant_spell_language_new:
 *
 * Create a new #EditorEnchantSpellLanguage.
 *
 * Returns: (transfer full): a newly created #EditorEnchantSpellLanguage
 */
EditorSpellLanguage *
editor_enchant_spell_language_new (const char *code,
                                   gpointer    native)
{
  return g_object_new (EDITOR_TYPE_ENCHANT_SPELL_LANGUAGE,
                       "code", code,
                       "native", native,
                       NULL);
}

static gboolean
editor_enchant_spell_language_contains_word (EditorSpellLanguage *language,
                                             const char          *word,
                                             gssize               word_len)
{
  EditorEnchantSpellLanguage *self = (EditorEnchantSpellLanguage *)language;

  g_assert (EDITOR_IS_ENCHANT_SPELL_LANGUAGE (self));
  g_assert (word != NULL);
  g_assert (word_len > 0);

  return enchant_dict_check (self->native, word, word_len) == 0;
}

static char **
editor_enchant_spell_language_list_corrections (EditorSpellLanguage *language,
                                                const char          *word,
                                                gssize               word_len)
{
  EditorEnchantSpellLanguage *self = (EditorEnchantSpellLanguage *)language;
  size_t count = 0;
  char **tmp;
  char **ret = NULL;

  g_assert (EDITOR_IS_ENCHANT_SPELL_LANGUAGE (self));
  g_assert (word != NULL);
  g_assert (word_len > 0);

  if ((tmp = enchant_dict_suggest (self->native, word, word_len, &count)) && count > 0)
    {
      ret = g_strdupv (tmp);
      enchant_dict_free_string_list (self->native, tmp);
    }

  return g_steal_pointer (&ret);
}

static char **
editor_enchant_spell_language_split (EditorEnchantSpellLanguage *self,
                                     const char                 *words)
{
  PangoLogAttr *attrs;
  GArray *ar;
  gsize n_chars;

  g_assert (EDITOR_IS_ENCHANT_SPELL_LANGUAGE (self));

  if (words == NULL || self->language == NULL)
    return NULL;

  /* We don't care about splitting obnoxious stuff */
  if ((n_chars = g_utf8_strlen (words, -1)) > 1024)
    return NULL;

  attrs = g_newa (PangoLogAttr, n_chars + 1);
  pango_get_log_attrs (words, -1, -1, self->language, attrs, n_chars + 1);

  ar = g_array_new (TRUE, FALSE, sizeof (char*));

  for (gsize i = 0; i < n_chars + 1; i++)
    {
      if (attrs[i].is_word_start)
        {
          for (gsize j = i + 1; j < n_chars + 1; j++)
            {
              if (attrs[j].is_word_end)
                {
                  char *substr = g_utf8_substring (words, i, j);
                  g_array_append_val (ar, substr);
                  i = j;
                  break;
                }
            }
        }
    }

  return (char **)(gpointer)g_array_free (ar, FALSE);
}

static void
editor_enchant_spell_language_add_all_to_session (EditorEnchantSpellLanguage *self,
                                                  const char * const         *words)
{
  g_assert (EDITOR_IS_ENCHANT_SPELL_LANGUAGE (self));

  if (words == NULL || words[0] == NULL)
    return;

  for (guint i = 0; words[i]; i++)
    enchant_dict_add_to_session (self->native, words[i], -1);
}

static void
editor_enchant_spell_language_add_word (EditorSpellLanguage *language,
                                        const char          *word)
{
  EditorEnchantSpellLanguage *self = (EditorEnchantSpellLanguage *)language;

  g_assert (EDITOR_IS_SPELL_LANGUAGE (language));
  g_assert (word != NULL);

  enchant_dict_add (self->native, word, -1);
}

static void
editor_enchant_spell_language_ignore_word (EditorSpellLanguage *language,
                                           const char          *word)
{
  EditorEnchantSpellLanguage *self = (EditorEnchantSpellLanguage *)language;

  g_assert (EDITOR_IS_SPELL_LANGUAGE (language));
  g_assert (word != NULL);

  enchant_dict_add_to_session (self->native, word, -1);
}

static const char *
editor_enchant_spell_language_get_extra_word_chars (EditorSpellLanguage *language)
{
  EditorEnchantSpellLanguage *self = (EditorEnchantSpellLanguage *)language;

  g_assert (EDITOR_IS_SPELL_LANGUAGE (language));

  return self->extra_word_chars;
}

static void
editor_enchant_spell_language_constructed (GObject *object)
{
  EditorEnchantSpellLanguage *self = (EditorEnchantSpellLanguage *)object;
  g_auto(GStrv) split = NULL;
  const char *extra_word_chars;
  const char *code;

  g_assert (EDITOR_IS_ENCHANT_SPELL_LANGUAGE (self));

  G_OBJECT_CLASS (editor_enchant_spell_language_parent_class)->constructed (object);

  code = editor_spell_language_get_code (EDITOR_SPELL_LANGUAGE (self));
  self->language = pango_language_from_string (code);

  if ((split = editor_enchant_spell_language_split (self, g_get_real_name ())))
    editor_enchant_spell_language_add_all_to_session (self, (const char * const *)split);

  if ((extra_word_chars = enchant_dict_get_extra_word_characters (self->native)))
    {
      const char *end_pos = NULL;

      /* Sometimes we get invalid UTF-8 from enchant, so handle that directly.
       * In particular, the data seems corrupted from Fedora.
       */
      if (g_utf8_validate (extra_word_chars, -1, &end_pos))
        self->extra_word_chars = g_strdup (extra_word_chars);
      else
        self->extra_word_chars = g_strndup (extra_word_chars, end_pos - extra_word_chars);
    }
}

static void
editor_enchant_spell_language_finalize (GObject *object)
{
  EditorEnchantSpellLanguage *self = (EditorEnchantSpellLanguage *)object;

  /* Owned by provider */
  self->native = NULL;

  G_OBJECT_CLASS (editor_enchant_spell_language_parent_class)->finalize (object);
}

static void
editor_enchant_spell_language_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  EditorEnchantSpellLanguage *self = EDITOR_ENCHANT_SPELL_LANGUAGE (object);

  switch (prop_id)
    {
    case PROP_NATIVE:
      g_value_set_pointer (value, self->native);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_enchant_spell_language_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  EditorEnchantSpellLanguage *self = EDITOR_ENCHANT_SPELL_LANGUAGE (object);

  switch (prop_id)
    {
    case PROP_NATIVE:
      self->native = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_enchant_spell_language_class_init (EditorEnchantSpellLanguageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  EditorSpellLanguageClass *spell_language_class = EDITOR_SPELL_LANGUAGE_CLASS (klass);

  object_class->constructed = editor_enchant_spell_language_constructed;
  object_class->finalize = editor_enchant_spell_language_finalize;
  object_class->get_property = editor_enchant_spell_language_get_property;
  object_class->set_property = editor_enchant_spell_language_set_property;

  spell_language_class->contains_word = editor_enchant_spell_language_contains_word;
  spell_language_class->list_corrections = editor_enchant_spell_language_list_corrections;
  spell_language_class->add_word = editor_enchant_spell_language_add_word;
  spell_language_class->ignore_word = editor_enchant_spell_language_ignore_word;
  spell_language_class->get_extra_word_chars = editor_enchant_spell_language_get_extra_word_chars;

  properties [PROP_NATIVE] =
    g_param_spec_pointer ("native",
                          "Native",
                          "The native enchant dictionary",
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
editor_enchant_spell_language_init (EditorEnchantSpellLanguage *self)
{
}

gpointer
editor_enchant_spell_language_get_native (EditorEnchantSpellLanguage *self)
{
  g_return_val_if_fail (EDITOR_IS_ENCHANT_SPELL_LANGUAGE (self), NULL);

  return self->native;
}
