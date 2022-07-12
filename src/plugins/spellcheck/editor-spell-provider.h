/* editor-spell-provider.h
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

#pragma once

#include "editor-types.h"

G_BEGIN_DECLS

#define EDITOR_TYPE_SPELL_PROVIDER (editor_spell_provider_get_type())

G_DECLARE_DERIVABLE_TYPE (EditorSpellProvider, editor_spell_provider, EDITOR, SPELL_PROVIDER, GObject)

struct _EditorSpellProviderClass
{
  GObjectClass parent_class;

  GPtrArray           *(*list_languages)    (EditorSpellProvider *self);
  gboolean             (*supports_language) (EditorSpellProvider *self,
                                             const char          *language);
  EditorSpellLanguage *(*get_language)      (EditorSpellProvider *self,
                                             const char          *language);
  const char          *(*get_default_code)  (EditorSpellProvider *self);

  /*< private >*/
  gpointer _reserved[8];
};

EditorSpellProvider *editor_spell_provider_get_default       (void);
const char          *editor_spell_provider_get_default_code  (EditorSpellProvider *self);
const char          *editor_spell_provider_get_display_name  (EditorSpellProvider *self);
gboolean             editor_spell_provider_supports_language (EditorSpellProvider *self,
                                                              const char          *language);
GPtrArray           *editor_spell_provider_list_languages    (EditorSpellProvider *self);
EditorSpellLanguage *editor_spell_provider_get_language      (EditorSpellProvider *self,
                                                              const char          *language);

G_END_DECLS
