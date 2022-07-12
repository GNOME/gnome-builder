/* editor-empty-spell-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

struct _EditorEmptySpellProvider
{
  EditorSpellProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (EditorEmptySpellProvider, editor_empty_spell_provider, EDITOR_TYPE_SPELL_PROVIDER)

EditorSpellProvider *
editor_empty_spell_provider_new (void)
{
  return g_object_new (EDITOR_TYPE_EMPTY_SPELL_PROVIDER, NULL);
}

static GPtrArray *
empty_list_languages (EditorSpellProvider *provider)
{
  return g_ptr_array_new_with_free_func (g_object_unref);
}

static EditorSpellLanguage *
empty_get_language (EditorSpellProvider *provider,
                    const char          *language)
{
  return NULL;
}

static gboolean
empty_supports_language (EditorSpellProvider *provider,
                         const char          *language)
{
  return FALSE;
}

static void
editor_empty_spell_provider_class_init (EditorEmptySpellProviderClass *klass)
{
  EditorSpellProviderClass *provider_class = EDITOR_SPELL_PROVIDER_CLASS (klass);

  provider_class->list_languages = empty_list_languages;
  provider_class->get_language = empty_get_language;
  provider_class->supports_language = empty_supports_language;
}

static void
editor_empty_spell_provider_init (EditorEmptySpellProvider *self)
{
}
