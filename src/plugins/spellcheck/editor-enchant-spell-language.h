/* editor-enchant-spell-language.h
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

#include "editor-spell-language.h"

G_BEGIN_DECLS

#define EDITOR_TYPE_ENCHANT_SPELL_LANGUAGE (editor_enchant_spell_language_get_type())

G_DECLARE_FINAL_TYPE (EditorEnchantSpellLanguage, editor_enchant_spell_language, EDITOR, ENCHANT_SPELL_LANGUAGE, EditorSpellLanguage)

EditorSpellLanguage *editor_enchant_spell_language_new        (const char                 *code,
                                                               gpointer                    native);
gpointer             editor_enchant_spell_language_get_native (EditorEnchantSpellLanguage *self);

G_END_DECLS
