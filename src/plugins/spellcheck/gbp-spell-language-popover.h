/* gbp-spell-language-popover.h
 *
 * Copyright 2017 Sébastien Lafargue <slafargue@gnome.org>
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

G_BEGIN_DECLS

#define GBP_TYPE_SPELL_LANGUAGE_POPOVER (gbp_spell_language_popover_get_type())

G_DECLARE_FINAL_TYPE (GbpSpellLanguagePopover, gbp_spell_language_popover, GBP, SPELL_LANGUAGE_POPOVER, GtkButton)

GbpSpellLanguagePopover *gbp_spell_language_popover_new (const GspellLanguage *language);

G_END_DECLS
