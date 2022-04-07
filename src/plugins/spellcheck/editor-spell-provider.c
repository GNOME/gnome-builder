/* editor-spell-provider.c
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

#include "editor-empty-spell-provider-private.h"
#include "editor-spell-provider.h"
#include "editor-enchant-spell-provider.h"

typedef struct
{
  char *display_name;
} EditorSpellProviderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (EditorSpellProvider, editor_spell_provider, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (EditorSpellProvider))

enum {
  PROP_0,
  PROP_DISPLAY_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
editor_spell_provider_finalize (GObject *object)
{
  EditorSpellProvider *self = (EditorSpellProvider *)object;
  EditorSpellProviderPrivate *priv = editor_spell_provider_get_instance_private (self);

  g_clear_pointer (&priv->display_name, g_free);

  G_OBJECT_CLASS (editor_spell_provider_parent_class)->finalize (object);
}

static void
editor_spell_provider_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  EditorSpellProvider *self = EDITOR_SPELL_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, editor_spell_provider_get_display_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_spell_provider_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  EditorSpellProvider *self = EDITOR_SPELL_PROVIDER (object);
  EditorSpellProviderPrivate *priv = editor_spell_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      priv->display_name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_spell_provider_class_init (EditorSpellProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = editor_spell_provider_finalize;
  object_class->get_property = editor_spell_provider_get_property;
  object_class->set_property = editor_spell_provider_set_property;

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "Display Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
editor_spell_provider_init (EditorSpellProvider *self)
{
}

const char *
editor_spell_provider_get_display_name (EditorSpellProvider *self)
{
  EditorSpellProviderPrivate *priv = editor_spell_provider_get_instance_private (self);

  g_return_val_if_fail (EDITOR_IS_SPELL_PROVIDER (self), NULL);

  return priv->display_name;
}

/**
 * editor_spell_provider_get_default:
 *
 * Gets the default spell provider.
 *
 * Returns: (transfer none): an #EditorSpellProvider
 */
EditorSpellProvider *
editor_spell_provider_get_default (void)
{
  static EditorSpellProvider *instance;

  if (instance == NULL)
    {
      instance = editor_enchant_spell_provider_new ();
      if (instance == NULL)
        instance = editor_empty_spell_provider_new ();

      g_set_weak_pointer (&instance, instance);
    }

  return instance;
}

/**
 * editor_spell_provider_supports_language:
 * @self: an #EditorSpellProvider
 * @language: the language such as `en_US`.
 *
 * Checks of @language is supported by the provider.
 *
 * Returns: %TRUE if @language is supported, otherwise %FALSE
 */
gboolean
editor_spell_provider_supports_language (EditorSpellProvider *self,
                                         const char          *language)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_PROVIDER (self), FALSE);
  g_return_val_if_fail (language != NULL, FALSE);

  return EDITOR_SPELL_PROVIDER_GET_CLASS (self)->supports_language (self, language);
}

/**
 * editor_spell_provider_list_languages:
 * @self: an #EditorSpellProvider
 *
 * Gets a list of the languages supported by the provider.
 *
 * Returns: (transfer container) (element-type EditorSpellLanguageInfo): an array of
 *   #EditorSpellLanguageInfo.
 */
GPtrArray *
editor_spell_provider_list_languages (EditorSpellProvider *self)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_PROVIDER (self), NULL);

  return EDITOR_SPELL_PROVIDER_GET_CLASS (self)->list_languages (self);
}

/**
 * editor_spell_provider_get_language:
 * @self: an #EditorSpellProvider
 * @language: the language to load such as `en_US`.
 *
 * Gets an #EditorSpellLanguage for the requested language, or %NULL
 * if the language is not supported.
 *
 * Returns: (transfer full) (nullable): an #EditorSpellProvider or %NULL
 */
EditorSpellLanguage *
editor_spell_provider_get_language (EditorSpellProvider *self,
                                    const char          *language)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_PROVIDER (self), NULL);
  g_return_val_if_fail (language != NULL, NULL);

  return EDITOR_SPELL_PROVIDER_GET_CLASS (self)->get_language (self, language);
}

const char *
editor_spell_provider_get_default_code (EditorSpellProvider *self)
{
  const char * const *langs;
  const char *ret;

  g_return_val_if_fail (EDITOR_IS_SPELL_PROVIDER (self), NULL);

  if (EDITOR_SPELL_PROVIDER_GET_CLASS (self)->get_default_code &&
      (ret = EDITOR_SPELL_PROVIDER_GET_CLASS (self)->get_default_code (self)))
    return ret;

  langs = g_get_language_names ();

  if (langs != NULL)
    {
      for (guint i = 0; langs[i]; i++)
        {
          /* Skip past things like "thing.utf8" since we'll
           * prefer to just have "thing" as it ensures we're
           * more likely to get code matches elsewhere.
           */
          if (strchr (langs[i], '.') != NULL)
            continue;

          if (editor_spell_provider_supports_language (self, langs[i]))
            return langs[i];
        }
    }

  if (editor_spell_provider_supports_language (self, "en_US"))
    return "en_US";

  if (editor_spell_provider_supports_language (self, "C"))
    return "C";

  return NULL;
}
