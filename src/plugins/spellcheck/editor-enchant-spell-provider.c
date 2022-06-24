/* editor-enchant-spell-provider.c
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

#include <glib/gi18n.h>
#include <enchant.h>
#include <locale.h>
#include <unicode/uloc.h>

#include "editor-spell-language-info.h"

#include "editor-enchant-spell-language.h"
#include "editor-enchant-spell-provider.h"

struct _EditorEnchantSpellProvider
{
  EditorSpellProvider parent_instance;
};

G_DEFINE_TYPE (EditorEnchantSpellProvider, editor_enchant_spell_provider, EDITOR_TYPE_SPELL_PROVIDER)

static GHashTable *languages;

static EnchantBroker *
get_broker (void)
{
  static EnchantBroker *broker;

  if (broker == NULL)
    broker = enchant_broker_init ();

  return broker;
}

static char *
_icu_uchar_to_char (const UChar *input,
                    gsize        max_input_len)
{
  GString *str;

  g_assert (input != NULL);
  g_assert (max_input_len > 0);

  if (input[0] == 0)
    return NULL;

  str = g_string_new (NULL);

  for (gsize i = 0; i < max_input_len; i++)
    {
      if (input[i] == 0)
        break;

      g_string_append_unichar (str, input[i]);
    }

  return g_string_free (str, FALSE);
}

static char *
get_display_name (const char *code)
{
  const char * const *names = g_get_language_names ();

  for (guint i = 0; names[i]; i++)
    {
      UChar ret[256];
      UErrorCode status = U_ZERO_ERROR;
      uloc_getDisplayName (code, names[i], ret, G_N_ELEMENTS (ret), &status);
      if (status == U_ZERO_ERROR)
        return _icu_uchar_to_char (ret, G_N_ELEMENTS (ret));
    }

  return NULL;
}

static char *
get_display_language (const char *code)
{
  const char * const *names = g_get_language_names ();

  for (guint i = 0; names[i]; i++)
    {
      UChar ret[256];
      UErrorCode status = U_ZERO_ERROR;
      uloc_getDisplayLanguage (code, names[i], ret, G_N_ELEMENTS (ret), &status);
      if (status == U_ZERO_ERROR)
        return _icu_uchar_to_char (ret, G_N_ELEMENTS (ret));
    }

  return NULL;
}

/**
 * editor_enchant_spell_provider_new:
 *
 * Create a new #EditorEnchantSpellProvider.
 *
 * Returns: (transfer full): a newly created #EditorEnchantSpellProvider
 */
EditorSpellProvider *
editor_enchant_spell_provider_new (void)
{
  return g_object_new (EDITOR_TYPE_ENCHANT_SPELL_PROVIDER,
                       "display-name", _("Enchant 2"),
                       NULL);
}

static gboolean
editor_enchant_spell_provider_supports_language (EditorSpellProvider *provider,
                                                 const char          *language)
{
  g_assert (EDITOR_IS_ENCHANT_SPELL_PROVIDER (provider));
  g_assert (language != NULL);

  return enchant_broker_dict_exists (get_broker (), language);
}

static void
list_languages_cb (const char * const  lang_tag,
                   const char * const  provider_name,
                   const char * const  provider_desc,
                   const char * const  provider_file,
                   void               *user_data)
{
  GPtrArray *ar = user_data;
  char *name = get_display_name (lang_tag);
  char *group = get_display_language (lang_tag);

  if (name != NULL)
    g_ptr_array_add (ar, editor_spell_language_info_new (name, lang_tag, group));

  g_free (name);
  g_free (group);
}

static GPtrArray *
editor_enchant_spell_provider_list_languages (EditorSpellProvider *provider)
{
  EnchantBroker *broker = get_broker ();
  GPtrArray *ar = g_ptr_array_new_with_free_func (g_object_unref);
  enchant_broker_list_dicts (broker, list_languages_cb, ar);
  return ar;
}

static EditorSpellLanguage *
editor_enchant_spell_provider_get_language (EditorSpellProvider *provider,
                                            const char          *language)
{
  EditorSpellLanguage *ret;

  g_assert (EDITOR_IS_ENCHANT_SPELL_PROVIDER (provider));
  g_assert (language != NULL);

  if (languages == NULL)
    languages = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  if (!(ret = g_hash_table_lookup (languages, language)))
    {
      EnchantDict *dict = enchant_broker_request_dict (get_broker (), language);

      if (dict == NULL)
        return NULL;

      ret = editor_enchant_spell_language_new (language, dict);
      g_hash_table_insert (languages, (char *)g_intern_string (language), ret);
    }

  return ret ? g_object_ref (ret) : NULL;
}

static void
editor_enchant_spell_provider_class_init (EditorEnchantSpellProviderClass *klass)
{
  EditorSpellProviderClass *spell_provider_class = EDITOR_SPELL_PROVIDER_CLASS (klass);

  spell_provider_class->supports_language = editor_enchant_spell_provider_supports_language;
  spell_provider_class->list_languages = editor_enchant_spell_provider_list_languages;
  spell_provider_class->get_language = editor_enchant_spell_provider_get_language;
}

static void
editor_enchant_spell_provider_init (EditorEnchantSpellProvider *self)
{
}
