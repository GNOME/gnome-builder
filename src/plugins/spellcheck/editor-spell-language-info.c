/* editor-spell-language-info.c
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

#include "editor-spell-language-info.h"

struct _EditorSpellLanguageInfo
{
  GObject parent_instance;
  char *name;
  char *code;
};

G_DEFINE_TYPE (EditorSpellLanguageInfo, editor_spell_language_info, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CODE,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * editor_spell_language_info_new:
 *
 * Create a new #EditorSpellLanguageInfo.
 *
 * Returns: (transfer full): a newly created #EditorSpellLanguageInfo
 */
EditorSpellLanguageInfo *
editor_spell_language_info_new (const char *name,
                                const char *code)
{
  return g_object_new (EDITOR_TYPE_SPELL_LANGUAGE_INFO,
                       "name", name,
                       "code", code,
                       NULL);
}

static void
editor_spell_language_info_finalize (GObject *object)
{
  EditorSpellLanguageInfo *self = (EditorSpellLanguageInfo *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->code, g_free);

  G_OBJECT_CLASS (editor_spell_language_info_parent_class)->finalize (object);
}

static void
editor_spell_language_info_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  EditorSpellLanguageInfo *self = EDITOR_SPELL_LANGUAGE_INFO (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, editor_spell_language_info_get_name (self));
      break;

    case PROP_CODE:
      g_value_set_string (value, editor_spell_language_info_get_code (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_spell_language_info_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  EditorSpellLanguageInfo *self = EDITOR_SPELL_LANGUAGE_INFO (object);

  switch (prop_id)
    {
    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_CODE:
      self->code = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
editor_spell_language_info_class_init (EditorSpellLanguageInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = editor_spell_language_info_finalize;
  object_class->get_property = editor_spell_language_info_get_property;
  object_class->set_property = editor_spell_language_info_set_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name of the language",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CODE] =
    g_param_spec_string ("code",
                         "Code",
                         "The language code",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
editor_spell_language_info_init (EditorSpellLanguageInfo *self)
{
}

const char *
editor_spell_language_info_get_name (EditorSpellLanguageInfo *self)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_LANGUAGE_INFO (self), NULL);

  return self->name;
}

const char *
editor_spell_language_info_get_code (EditorSpellLanguageInfo *self)
{
  g_return_val_if_fail (EDITOR_IS_SPELL_LANGUAGE_INFO (self), NULL);

  return self->code;
}
