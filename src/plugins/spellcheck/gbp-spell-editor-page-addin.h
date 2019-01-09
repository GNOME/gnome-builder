/* gbp-spell-editor-page-addin.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include <gspell/gspell.h>
#include <libide-editor.h>

G_BEGIN_DECLS

#define GBP_TYPE_SPELL_EDITOR_PAGE_ADDIN (gbp_spell_editor_page_addin_get_type())

G_DECLARE_FINAL_TYPE (GbpSpellEditorPageAddin, gbp_spell_editor_page_addin, GBP, SPELL_EDITOR_PAGE_ADDIN, GObject)

void             gbp_spell_editor_page_addin_begin_checking     (GbpSpellEditorPageAddin *self);
void             gbp_spell_editor_page_addin_end_checking       (GbpSpellEditorPageAddin *self);
GspellChecker   *gbp_spell_editor_page_addin_get_checker        (GbpSpellEditorPageAddin *self);
GspellNavigator *gbp_spell_editor_page_addin_get_navigator      (GbpSpellEditorPageAddin *self);
guint            gbp_spell_editor_page_addin_get_count          (GbpSpellEditorPageAddin *self,
                                                                 const gchar             *word);
gboolean         gbp_spell_editor_page_addin_move_to_word_start (GbpSpellEditorPageAddin *self);

G_END_DECLS
