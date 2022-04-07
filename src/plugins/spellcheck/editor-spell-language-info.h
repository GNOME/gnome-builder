/* editor-spell-language-info.h
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

#define EDITOR_TYPE_SPELL_LANGUAGE_INFO (editor_spell_language_info_get_type())

G_DECLARE_FINAL_TYPE (EditorSpellLanguageInfo, editor_spell_language_info, EDITOR, SPELL_LANGUAGE_INFO, GObject)

EditorSpellLanguageInfo *editor_spell_language_info_new      (const char              *name,
                                                              const char              *code);
const char              *editor_spell_language_info_get_name (EditorSpellLanguageInfo *self);
const char              *editor_spell_language_info_get_code (EditorSpellLanguageInfo *self);

G_END_DECLS
