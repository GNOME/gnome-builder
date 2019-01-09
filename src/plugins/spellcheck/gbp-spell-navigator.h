/* gbp-spell-navigator.h
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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

#define GBP_TYPE_SPELL_NAVIGATOR (gbp_spell_navigator_get_type())

G_DECLARE_FINAL_TYPE (GbpSpellNavigator, gbp_spell_navigator, GBP, SPELL_NAVIGATOR, GInitiallyUnowned)

GspellNavigator *gbp_spell_navigator_new                   (GtkTextView       *view);
guint            gbp_spell_navigator_get_count             (GbpSpellNavigator *self,
                                                            const gchar       *word);
gboolean         gbp_spell_navigator_get_is_words_counted  (GbpSpellNavigator *self);
gboolean         gbp_spell_navigator_goto_word_start       (GbpSpellNavigator *self);

G_END_DECLS
