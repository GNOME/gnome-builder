/* gbp-spell-buffer-addin.h
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

#include <libide-editor.h>
#include <gspell/gspell.h>

G_BEGIN_DECLS

#define GBP_TYPE_SPELL_BUFFER_ADDIN (gbp_spell_buffer_addin_get_type())

G_DECLARE_FINAL_TYPE (GbpSpellBufferAddin, gbp_spell_buffer_addin, GBP, SPELL_BUFFER_ADDIN, GObject)

GspellChecker *gbp_spell_buffer_addin_get_checker        (GbpSpellBufferAddin *self);
void           gbp_spell_buffer_addin_begin_checking     (GbpSpellBufferAddin *self);
void           gbp_spell_buffer_addin_end_checking       (GbpSpellBufferAddin *self);
GtkTextTag    *gbp_spell_buffer_addin_get_misspelled_tag (GbpSpellBufferAddin *self);

G_END_DECLS
