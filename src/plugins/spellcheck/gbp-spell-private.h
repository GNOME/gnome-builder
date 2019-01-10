/* gbp-spell-widget-private.h
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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

#include "gbp-spell-dict.h"
#include "gbp-spell-widget.h"
#include "gbp-spell-editor-addin.h"
#include "gbp-spell-editor-page-addin.h"

G_BEGIN_DECLS

typedef enum
{
  CHECK_WORD_NONE,
  CHECK_WORD_CHECKING,
  CHECK_WORD_IDLE
} CheckWordState;

struct _GbpSpellWidget
{
  GtkBin                   parent_instance;

  /* Owned references */
  IdeEditorPage           *editor;
  GbpSpellEditorPageAddin *editor_page_addin;
  DzlSignalGroup          *editor_page_addin_signals;
  GPtrArray               *words_array;
  GbpSpellDict            *dict;

  /* Borrowed references */
  const GspellLanguage    *language;

  /* Template references */
  GtkLabel                *word_label;
  GtkLabel                *count_label;
  GtkEntry                *word_entry;
  GtkListBox              *suggestions_box;
  GtkBox                  *count_box;
  GtkWidget               *dict_word_entry;
  GtkWidget               *dict_add_button;
  GtkWidget               *dict_words_list;
  GtkButton               *language_chooser_button;
  GtkButton               *close_button;
  GtkWidget               *placeholder;

  /* GSource identifiers */
  guint                    check_word_timeout_id;
  guint                    dict_check_word_timeout_id;

  guint                    current_word_count;
  CheckWordState           check_word_state;
  CheckWordState           dict_check_word_state;

  guint                    is_checking_word : 1;
  guint                    is_check_word_invalid : 1;
  guint                    is_check_word_idle : 1;
  guint                    is_word_entry_valid : 1;

  guint                    is_dict_checking_word : 1;
  guint                    is_dict_check_word_invalid : 1;
  guint                    is_dict_check_word_idle : 1;

  guint                    spellchecking_status : 1;
};

void       _gbp_spell_widget_init_actions   (GbpSpellWidget *self);
void       _gbp_spell_widget_update_actions (GbpSpellWidget *self);
gboolean   _gbp_spell_widget_move_next_word (GbpSpellWidget *self);
GtkWidget *_gbp_spell_widget_get_entry      (GbpSpellWidget *self);
void       _gbp_spell_widget_change         (GbpSpellWidget *self,
                                             gboolean        change_all);

void       _gbp_spell_editor_addin_begin    (GbpSpellEditorAddin *self,
                                             IdeEditorPage       *view);
void       _gbp_spell_editor_addin_cancel   (GbpSpellEditorAddin *self,
                                             IdeEditorPage       *view);

G_END_DECLS
