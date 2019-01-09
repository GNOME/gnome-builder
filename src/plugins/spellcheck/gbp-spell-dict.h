/* gbp-spell-dict.h
 *
 * Copyright 2016 Sébastien Lafargue <slafargue@gnome.org>
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

#include <glib-object.h>
#include <gspell/gspell.h>

G_BEGIN_DECLS

#define GBP_TYPE_SPELL_DICT (gbp_spell_dict_get_type())

G_DECLARE_FINAL_TYPE (GbpSpellDict, gbp_spell_dict, GBP, SPELL_DICT, GObject)

GbpSpellDict  *gbp_spell_dict_new                       (GspellChecker *checker);
GspellChecker *gbp_spell_dict_get_checker               (GbpSpellDict  *self);
GPtrArray     *gbp_spell_dict_get_words                 (GbpSpellDict  *self);
void           gbp_spell_dict_set_checker               (GbpSpellDict  *self,
                                                         GspellChecker *checker);
gboolean       gbp_spell_dict_add_word_to_personal      (GbpSpellDict  *self,
                                                         const gchar   *word);
gboolean       gbp_spell_dict_remove_word_from_personal (GbpSpellDict  *self,
                                                         const gchar   *word);
gboolean       gbp_spell_dict_personal_contains         (GbpSpellDict  *self,
                                                         const gchar   *word);

G_END_DECLS
