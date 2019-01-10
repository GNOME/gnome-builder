/* gbp-spell-widget.c
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

#define G_LOG_DOMAIN "gbp-spell-widget"

#include <dazzle.h>
#include <libide-editor.h>
#include <glib/gi18n.h>
#include <gspell/gspell.h>

#include "gbp-spell-dict.h"
#include "gbp-spell-language-popover.h"
#include "gbp-spell-navigator.h"
#include "gbp-spell-private.h"
#include "gbp-spell-widget.h"

G_DEFINE_TYPE (GbpSpellWidget, gbp_spell_widget, GTK_TYPE_BIN)

#define CHECK_WORD_INTERVAL_MIN      100
#define DICT_CHECK_WORD_INTERVAL_MIN 100
#define WORD_ENTRY_MAX_SUGGESTIONS   6

enum {
  PROP_0,
  PROP_EDITOR,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
clear_suggestions_box (GbpSpellWidget *self)
{
  g_assert (GBP_IS_SPELL_WIDGET (self));

  gtk_container_foreach (GTK_CONTAINER (self->suggestions_box),
                         (GtkCallback)gtk_widget_destroy,
                         NULL);
}

static void
update_global_sensiblility (GbpSpellWidget *self,
                            gboolean        sensibility)
{
  g_assert (GBP_IS_SPELL_WIDGET (self));

  gtk_entry_set_text (self->word_entry, "");
  clear_suggestions_box (self);
  _gbp_spell_widget_update_actions (self);
}

GtkWidget *
_gbp_spell_widget_get_entry (GbpSpellWidget *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_WIDGET (self), NULL);

  return GTK_WIDGET (self->word_entry);
}

static GtkWidget *
create_suggestion_row (GbpSpellWidget *self,
                       const gchar    *word)
{
  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (!dzl_str_empty0 (word));

  return g_object_new (GTK_TYPE_LABEL,
                       "label", word,
                       "visible", TRUE,
                       "xalign", 0.0f,
                       NULL);
}

static void
fill_suggestions_box (GbpSpellWidget  *self,
                      const gchar     *word,
                      gchar          **first_result)
{
  GspellChecker *checker;
  GSList *suggestions = NULL;
  GtkWidget *item;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (first_result != NULL);

  *first_result = NULL;

  clear_suggestions_box (self);

  if (dzl_str_empty0 (word))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->suggestions_box), FALSE);
      return;
    }

  if (self->editor_page_addin != NULL)
    {
      checker = gbp_spell_editor_page_addin_get_checker (self->editor_page_addin);
      suggestions = gspell_checker_get_suggestions (checker, word, -1);
    }

  if (suggestions == NULL)
    {
      gtk_label_set_text (GTK_LABEL (self->placeholder), _("No suggestions"));
      gtk_widget_set_sensitive (GTK_WIDGET (self->suggestions_box), FALSE);
    }
  else
    {
      *first_result = g_strdup (suggestions->data);

      gtk_widget_set_sensitive (GTK_WIDGET (self->suggestions_box), TRUE);

      for (const GSList *iter = suggestions; iter; iter = iter->next)
        {
          const gchar *iter_word = iter->data;
          item = create_suggestion_row (self, iter_word);
          gtk_list_box_insert (self->suggestions_box, item, -1);
        }

      g_slist_free_full (suggestions, g_free);
    }
}

static void
update_count_label (GbpSpellWidget *self)
{
  GspellNavigator *navigator;
  const gchar *word;
  guint count;

  g_assert (GBP_IS_SPELL_WIDGET (self));

  if (self->editor_page_addin == NULL)
    return;

  navigator = gbp_spell_editor_page_addin_get_navigator (self->editor_page_addin);
  word = gtk_label_get_text (self->word_label);
  count = gbp_spell_navigator_get_count (GBP_SPELL_NAVIGATOR (navigator), word);

  if (count > 0)
    {
      g_autofree gchar *count_text = NULL;

      if (count > 1000)
        count_text = g_strdup (">1000");
      else
        count_text = g_strdup_printf ("%i", count);

      gtk_label_set_text (self->count_label, count_text);
      gtk_widget_set_visible (GTK_WIDGET (self->count_box), TRUE);
    }
  else
    gtk_widget_set_visible (GTK_WIDGET (self->count_box), TRUE);

  self->current_word_count = count;

  _gbp_spell_widget_update_actions (self);
}

gboolean
_gbp_spell_widget_move_next_word (GbpSpellWidget *self)
{
  g_autofree gchar *word = NULL;
  g_autofree gchar *first_result = NULL;
  g_autoptr(GError) error = NULL;
  GspellNavigator *navigator;
  GtkListBoxRow *row;
  gboolean ret = FALSE;

  g_assert (GBP_IS_SPELL_WIDGET (self));

  if (self->editor_page_addin == NULL)
    return FALSE;

  navigator = gbp_spell_editor_page_addin_get_navigator (self->editor_page_addin);

  if ((ret = gspell_navigator_goto_next (navigator, &word, NULL, &error)))
    {
      gtk_label_set_text (self->word_label, word);
      update_count_label (self);

      fill_suggestions_box (self, word, &first_result);

      if (!dzl_str_empty0 (first_result))
        {
          row = gtk_list_box_get_row_at_index (self->suggestions_box, 0);
          gtk_list_box_select_row (self->suggestions_box, row);
        }
    }
  else
    {
      if (error != NULL)
        gtk_label_set_text (GTK_LABEL (self->placeholder), error->message);

      self->spellchecking_status = FALSE;

      gtk_label_set_text (GTK_LABEL (self->placeholder), _("Completed spell checking"));
      update_global_sensiblility (self, FALSE);
    }

  _gbp_spell_widget_update_actions (self);

  return ret;
}

static gboolean
check_word_timeout_cb (GbpSpellWidget *self)
{
  g_autoptr(GError) error = NULL;
  GspellChecker *checker;
  const gchar *icon_name = "";
  const gchar *word;
  gboolean ret = TRUE;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (self->editor_page_addin != NULL);

  checker = gbp_spell_editor_page_addin_get_checker (self->editor_page_addin);

  self->check_word_state = CHECK_WORD_CHECKING;

  word = gtk_entry_get_text (self->word_entry);

  if (!dzl_str_empty0 (word))
    {
      /* FIXME: suggestions can give a multiple-words suggestion
       * that failed to the checkword test, ex: auto tools
       */
      ret = gspell_checker_check_word (checker, word, -1, &error);
      if (error != NULL)
        {
          g_message ("check error:%s\n", error->message);
        }
    }

  if (!ret)
    {
      icon_name = "dialog-warning-symbolic";
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->word_entry),
                                   _("The word is not in the dictionary"));
    }
  else
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->word_entry), NULL);

  gtk_entry_set_icon_from_icon_name (self->word_entry,
                                     GTK_ENTRY_ICON_SECONDARY,
                                     icon_name);

  self->check_word_state = CHECK_WORD_NONE;
  self->is_word_entry_valid = ret;

  self->check_word_timeout_id = 0;

  if (self->is_check_word_invalid == TRUE)
    {
      self->check_word_timeout_id = g_timeout_add_full (G_PRIORITY_LOW,
                                                        CHECK_WORD_INTERVAL_MIN,
                                                        (GSourceFunc)check_word_timeout_cb,
                                                        g_object_ref (self),
                                                        g_object_unref);
      self->check_word_state = CHECK_WORD_IDLE;
      self->is_check_word_invalid = FALSE;
    }

  return G_SOURCE_REMOVE;
}

static void
gbp_spell_widget__word_entry_changed_cb (GbpSpellWidget *self,
                                         GtkEntry       *entry)
{
  const gchar *word;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_ENTRY (entry));

  _gbp_spell_widget_update_actions (self);

  word = gtk_entry_get_text (self->word_entry);
  if (dzl_str_empty0 (word) && self->spellchecking_status == TRUE)
    {
      word = gtk_label_get_text (self->word_label);
      gtk_entry_set_text (GTK_ENTRY (self->dict_word_entry), word);
    }
  else
    gtk_entry_set_text (GTK_ENTRY (self->dict_word_entry), word);

  if (self->check_word_state == CHECK_WORD_CHECKING)
    {
      self->is_check_word_invalid = TRUE;
      return;
    }

  dzl_clear_source (&self->check_word_timeout_id);

  if (self->editor_page_addin != NULL)
    {
      self->check_word_timeout_id = g_timeout_add_full (G_PRIORITY_LOW,
                                                        CHECK_WORD_INTERVAL_MIN,
                                                        (GSourceFunc)check_word_timeout_cb,
                                                        g_object_ref (self),
                                                        g_object_unref);
      self->check_word_state = CHECK_WORD_IDLE;
    }
}

static void
gbp_spell_widget__row_selected_cb (GbpSpellWidget *self,
                                   GtkListBoxRow  *row,
                                   GtkListBox     *listbox)
{
  const gchar *word;
  GtkLabel *label;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row) || row == NULL);
  g_assert (GTK_IS_LIST_BOX (listbox));

  if (row != NULL)
    {
      label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (row)));
      word = gtk_label_get_text (label);

      g_signal_handlers_block_by_func (self->word_entry, gbp_spell_widget__word_entry_changed_cb, self);

      gtk_entry_set_text (self->word_entry, word);
      gtk_editable_set_position (GTK_EDITABLE (self->word_entry), -1);
      _gbp_spell_widget_update_actions (self);

      g_signal_handlers_unblock_by_func (self->word_entry, gbp_spell_widget__word_entry_changed_cb, self);
    }
}

static void
gbp_spell_widget__row_activated_cb (GbpSpellWidget *self,
                                    GtkListBoxRow  *row,
                                    GtkListBox     *listbox)
{
  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (listbox));

  if (row != NULL)
    _gbp_spell_widget_change (self, FALSE);
}

static void
gbp_spell_widget__words_counted_cb (GbpSpellWidget  *self,
                                    GParamSpec      *pspec,
                                    GspellNavigator *navigator)
{
  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GSPELL_IS_NAVIGATOR (navigator));

  update_count_label (self);
}

static GtkListBoxRow *
get_next_row_to_focus (GtkListBox    *listbox,
                       GtkListBoxRow *row)
{
  g_autoptr(GList) children = NULL;
  gint index;
  gint new_index;
  gint len;

  g_assert (GTK_IS_LIST_BOX (listbox));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  children = gtk_container_get_children (GTK_CONTAINER (listbox));
  if (0 == (len = g_list_length (children)))
    return NULL;

  index = gtk_list_box_row_get_index (row);
  if (index < len - 1)
    new_index = index + 1;
  else if (index == len - 1 && len > 1)
    new_index = index - 1;
  else
    return NULL;

  return gtk_list_box_get_row_at_index (listbox, new_index);
}

static gboolean
dict_check_word_timeout_cb (GbpSpellWidget *self)
{
  g_autofree gchar *tooltip = NULL;
  GspellChecker *checker;
  const gchar *icon_name = "";
  const gchar *word;
  gboolean valid = FALSE;

  g_assert (GBP_IS_SPELL_WIDGET (self));

  if (self->editor_page_addin == NULL)
    {
      /* lost our chance */
      self->dict_check_word_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  checker = gbp_spell_editor_page_addin_get_checker (self->editor_page_addin);

  self->dict_check_word_state = CHECK_WORD_CHECKING;

  word = gtk_entry_get_text (GTK_ENTRY (self->dict_word_entry));
  if (!dzl_str_empty0 (word))
    {
      if (gbp_spell_dict_personal_contains (self->dict, word))
        gtk_widget_set_tooltip_text (self->dict_word_entry, _("This word is already in the personal dictionary"));
      else if (gspell_checker_check_word (checker, word, -1, NULL))
        {
          tooltip = g_strdup_printf (_("This word is already in the %s dictionary"), gspell_language_get_name (self->language));
          gtk_widget_set_tooltip_text (self->dict_word_entry, tooltip);
        }
      else
        {
          valid = TRUE;
          gtk_widget_set_tooltip_text (self->dict_word_entry, NULL);
        }

      if (!valid)
        icon_name = "dialog-warning-symbolic";
    }

  gtk_widget_set_sensitive (GTK_WIDGET (self->dict_add_button), valid);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self->dict_word_entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     icon_name);

  self->dict_check_word_state = CHECK_WORD_NONE;

  self->dict_check_word_timeout_id = 0;
  if (self->is_dict_check_word_invalid == TRUE)
    {
      self->dict_check_word_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                                             DICT_CHECK_WORD_INTERVAL_MIN,
                                                             (GSourceFunc)dict_check_word_timeout_cb,
                                                             self,
                                                             NULL);
      self->dict_check_word_state = CHECK_WORD_IDLE;
      self->is_dict_check_word_invalid = FALSE;
    }

  return G_SOURCE_REMOVE;
}

static void
gbp_spell_widget__dict_word_entry_changed_cb (GbpSpellWidget *self,
                                              GtkEntry       *dict_word_entry)
{
  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_ENTRY (dict_word_entry));

  if (self->dict_check_word_state == CHECK_WORD_CHECKING)
    {
      self->is_dict_check_word_invalid = TRUE;
      return;
    }

  if (self->dict_check_word_state == CHECK_WORD_IDLE)
    {
      g_source_remove (self->dict_check_word_timeout_id);
      self->dict_check_word_timeout_id = 0;
    }

  self->dict_check_word_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                                         CHECK_WORD_INTERVAL_MIN,
                                                         (GSourceFunc)dict_check_word_timeout_cb,
                                                         self,
                                                         NULL);
  self->dict_check_word_state = CHECK_WORD_IDLE;
}

static void
remove_dict_row (GbpSpellWidget *self,
                 GtkListBox     *listbox,
                 GtkListBoxRow  *row)
{
  GtkListBoxRow *next_row;
  gchar *word;
  gboolean exist;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_LIST_BOX (listbox));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  word = g_object_get_data (G_OBJECT (row), "word");
  exist = gbp_spell_dict_remove_word_from_personal (self->dict, word);
  if (!exist)
    g_warning ("The word %s do not exist in the personnal dictionary", word);

  if (row == gtk_list_box_get_selected_row (listbox))
    {
      if (NULL != (next_row = get_next_row_to_focus (listbox, row)))
        {
          gtk_widget_grab_focus (GTK_WIDGET (next_row));
          gtk_list_box_select_row (listbox, next_row);
        }
      else
        gtk_widget_grab_focus (GTK_WIDGET (self->word_entry));
    }

  gtk_container_remove (GTK_CONTAINER (self->dict_words_list), GTK_WIDGET (row));
  gbp_spell_widget__dict_word_entry_changed_cb (self, GTK_ENTRY (self->dict_word_entry));
}

static void
dict_close_button_clicked_cb (GbpSpellWidget *self,
                              GtkButton      *button)
{
  GtkWidget *row;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  if (NULL != (row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW)))
    remove_dict_row (self, GTK_LIST_BOX (self->dict_words_list), GTK_LIST_BOX_ROW (row));
}

static gboolean
dict_row_key_pressed_event_cb (GbpSpellWidget *self,
                               GdkEventKey    *event,
                               GtkListBox     *listbox)
{
  GtkListBoxRow *row;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_LIST_BOX (listbox));

  if (event->keyval == GDK_KEY_Delete &&
      NULL != (row = gtk_list_box_get_selected_row (listbox)))
    {
      remove_dict_row (self, GTK_LIST_BOX (self->dict_words_list), GTK_LIST_BOX_ROW (row));
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static GtkWidget *
dict_create_word_row (GbpSpellWidget *self,
                      const gchar    *word)
{
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *button;
  GtkStyleContext *style_context;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (!dzl_str_empty0 (word));

  label = g_object_new (GTK_TYPE_LABEL,
                       "label", word,
                       "halign", GTK_ALIGN_START,
                        "visible", TRUE,
                       NULL);

  button = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_visible (button, TRUE);
  gtk_widget_set_can_focus (button, FALSE);
  g_signal_connect_swapped (button,
                            "clicked",
                            G_CALLBACK (dict_close_button_clicked_cb),
                            self);

  style_context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (style_context, "close");

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "expand", TRUE,
                      "spacing", 6,
                      "visible", TRUE,
                      NULL);

  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);

  row = gtk_list_box_row_new ();
  gtk_widget_set_visible (row, TRUE);

  gtk_container_add (GTK_CONTAINER (row), box);
  g_object_set_data_full (G_OBJECT (row), "word", g_strdup (word), g_free);

  return row;
}

static gboolean
check_dict_available (GbpSpellWidget *self)
{
  g_assert (GBP_IS_SPELL_WIDGET (self));

  return (self->editor_page_addin != NULL && self->language != NULL);
}

static void
gbp_spell_widget__add_button_clicked_cb (GbpSpellWidget *self,
                                         GtkButton      *button)
{
  const gchar *word;
  GtkWidget *item;
  GtkWidget *toplevel;
  GtkWidget *focused_widget;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  word = gtk_entry_get_text (GTK_ENTRY (self->dict_word_entry));

  /* TODO: check if word already in dict */
  if (check_dict_available (self) && !dzl_str_empty0 (word))
    {
      if (!gbp_spell_dict_add_word_to_personal (self->dict, word))
        return;

      item = dict_create_word_row (self, word);
      gtk_list_box_insert (GTK_LIST_BOX (self->dict_words_list), item, 0);

      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
      if (GTK_IS_WINDOW (toplevel) &&
          NULL != (focused_widget = gtk_window_get_focus (GTK_WINDOW (toplevel))))
        {
          if (focused_widget != GTK_WIDGET (self->word_entry) &&
              focused_widget != self->dict_word_entry)
            gtk_widget_grab_focus (self->dict_word_entry);
        }

      gtk_entry_set_text (GTK_ENTRY (self->dict_word_entry), "");
    }
}

static void
dict_clean_listbox (GbpSpellWidget *self)
{
  GList *children;

  g_assert (GBP_IS_SPELL_WIDGET (self));

  children = gtk_container_get_children (GTK_CONTAINER (self->dict_words_list));
  for (GList *l = children; l != NULL; l = g_list_next (l))
    gtk_widget_destroy (GTK_WIDGET (l->data));
}

static void
dict_fill_listbox (GbpSpellWidget *self,
                   GPtrArray      *words_array)
{
  const gchar *word;
  GtkWidget *item;
  guint len;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (words_array != NULL);

  dict_clean_listbox (self);

  len = words_array->len;
  for (guint i = 0; i < len; ++i)
    {
      word = g_ptr_array_index (words_array, i);
      item = dict_create_word_row (self, word);
      gtk_list_box_insert (GTK_LIST_BOX (self->dict_words_list), item, -1);
    }
}

static void
gbp_spell_widget__language_notify_cb (GbpSpellWidget *self,
                                      GParamSpec     *pspec,
                                      GtkButton      *language_chooser_button)
{
  const GspellLanguage *current_language;
  const GspellLanguage *spell_language;
  g_autofree gchar *word = NULL;
  g_autofree gchar *first_result = NULL;
  GspellNavigator *navigator;
  GspellChecker *checker;
  GtkListBoxRow *row;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_BUTTON (language_chooser_button));

  if (self->editor_page_addin == NULL)
    return;

  checker = gbp_spell_editor_page_addin_get_checker (self->editor_page_addin);
  navigator = gbp_spell_editor_page_addin_get_navigator (self->editor_page_addin);

  current_language = gspell_checker_get_language (checker);
  spell_language = gspell_language_chooser_get_language (GSPELL_LANGUAGE_CHOOSER (language_chooser_button));
  if (gspell_language_compare (current_language, spell_language) != 0)
    {
      gspell_checker_set_language (checker, spell_language);
      fill_suggestions_box (self, word, &first_result);
      if (!dzl_str_empty0 (first_result))
        {
          row = gtk_list_box_get_row_at_index (self->suggestions_box, 0);
          gtk_list_box_select_row (self->suggestions_box, row);
        }

      g_clear_pointer (&self->words_array, g_ptr_array_unref);

      if (current_language == NULL)
        {
          dict_clean_listbox (self);
          gtk_widget_set_sensitive (GTK_WIDGET (self->dict_add_button), FALSE);
          gtk_widget_set_sensitive (GTK_WIDGET (self->dict_words_list), FALSE);

          return;
        }

      gbp_spell_widget__dict_word_entry_changed_cb (self, GTK_ENTRY (self->dict_word_entry));
      gtk_widget_set_sensitive (GTK_WIDGET (self->dict_words_list), TRUE);

      gbp_spell_navigator_goto_word_start (GBP_SPELL_NAVIGATOR (navigator));

      _gbp_spell_widget_move_next_word (self);
    }
}

static void
gbp_spell_widget__word_entry_suggestion_activate (GbpSpellWidget *self,
                                                  GtkMenuItem    *item)
{
  gchar *word;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_MENU_ITEM (item));

  word = g_object_get_data (G_OBJECT (item), "word");

  g_signal_handlers_block_by_func (self->word_entry, gbp_spell_widget__word_entry_changed_cb, self);

  gtk_entry_set_text (self->word_entry, word);
  gtk_editable_set_position (GTK_EDITABLE (self->word_entry), -1);
  _gbp_spell_widget_update_actions (self);

  g_signal_handlers_unblock_by_func (self->word_entry, gbp_spell_widget__word_entry_changed_cb, self);
}

static void
gbp_spell_widget__populate_popup_cb (GbpSpellWidget *self,
                                     GtkWidget      *popup,
                                     GtkEntry       *entry)
{
  GSList *suggestions = NULL;
  GspellChecker *checker;
  const gchar *text;
  GtkWidget *item;
  guint count = 0;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_WIDGET (popup));
  g_assert (GTK_IS_ENTRY (entry));

  if (self->editor_page_addin == NULL)
    return;

  checker = gbp_spell_editor_page_addin_get_checker (self->editor_page_addin);
  text = gtk_entry_get_text (entry);

  if (!self->is_word_entry_valid && !dzl_str_empty0 (text))
    suggestions = gspell_checker_get_suggestions (checker, text, -1);

  if (suggestions == NULL)
    return;

  item = g_object_new (GTK_TYPE_SEPARATOR_MENU_ITEM,
                       "visible", TRUE,
                       NULL);
  gtk_menu_shell_prepend (GTK_MENU_SHELL (popup), item);

  suggestions = g_slist_reverse (suggestions);

  for (const GSList *iter = suggestions; iter; iter = iter->next)
    {
      const gchar *word = iter->data;

      item = g_object_new (GTK_TYPE_MENU_ITEM,
                           "label", word,
                           "visible", TRUE,
                           NULL);
      g_object_set_data (G_OBJECT (item), "word", g_strdup (word));
      gtk_menu_shell_prepend (GTK_MENU_SHELL (popup), item);
      g_signal_connect_object (item,
                               "activate",
                               G_CALLBACK (gbp_spell_widget__word_entry_suggestion_activate),
                               self,
                               G_CONNECT_SWAPPED);

      if (++count >= WORD_ENTRY_MAX_SUGGESTIONS)
        break;
    }

  g_slist_free_full (suggestions, g_free);
}

static void
gbp_spell_widget__dict__loaded_cb (GbpSpellWidget *self,
                                   GbpSpellDict   *dict)
{
  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GBP_IS_SPELL_DICT (dict));

  self->words_array = gbp_spell_dict_get_words (self->dict);
  dict_fill_listbox (self, self->words_array);
  g_clear_pointer (&self->words_array, g_ptr_array_unref);
}

static void
gbp_spell_widget__word_label_notify_cb (GbpSpellWidget *self,
                                        GParamSpec     *pspec,
                                        GtkLabel       *word_label)
{
  const gchar *text;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_LABEL (word_label));

  if (self->spellchecking_status == TRUE)
    text = gtk_label_get_text (word_label);
  else
    text = "";

  gtk_entry_set_text (GTK_ENTRY (self->dict_word_entry), text);
}

static void
gbp_spell_widget__close_button_clicked_cb (GbpSpellWidget *self,
                                           GtkButton      *close_button)
{
  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GTK_IS_BUTTON (close_button));

  gbp_spell_widget_set_editor (self, NULL);
}

static void
gbp_spell_widget_constructed (GObject *object)
{
  GbpSpellWidget *self = (GbpSpellWidget *)object;

  _gbp_spell_widget_init_actions (self);
  gbp_spell_widget__word_entry_changed_cb (self, self->word_entry);

  g_signal_connect_swapped (self->word_entry,
                            "changed",
                            G_CALLBACK (gbp_spell_widget__word_entry_changed_cb),
                            self);

  g_signal_connect_swapped (self->word_entry,
                            "populate-popup",
                            G_CALLBACK (gbp_spell_widget__populate_popup_cb),
                            self);

  g_signal_connect_swapped (self->suggestions_box,
                            "row-selected",
                            G_CALLBACK (gbp_spell_widget__row_selected_cb),
                            self);

  g_signal_connect_swapped (self->suggestions_box,
                            "row-activated",
                            G_CALLBACK (gbp_spell_widget__row_activated_cb),
                            self);

  g_signal_connect_object (self->language_chooser_button,
                           "notify::language",
                           G_CALLBACK (gbp_spell_widget__language_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_swapped (self->dict_add_button,
                            "clicked",
                            G_CALLBACK (gbp_spell_widget__add_button_clicked_cb),
                            self);

  g_signal_connect_swapped (self->dict_word_entry,
                            "changed",
                            G_CALLBACK (gbp_spell_widget__dict_word_entry_changed_cb),
                            self);

  g_signal_connect_swapped (self->close_button,
                            "clicked",
                            G_CALLBACK (gbp_spell_widget__close_button_clicked_cb),
                            self);

  self->placeholder = gtk_label_new (NULL);
  gtk_widget_set_visible (self->placeholder, TRUE);
  gtk_list_box_set_placeholder (self->suggestions_box, self->placeholder);

  g_signal_connect_swapped (self->dict,
                            "loaded",
                            G_CALLBACK (gbp_spell_widget__dict__loaded_cb),
                            self);

  g_signal_connect_object (self->word_label,
                           "notify::label",
                           G_CALLBACK (gbp_spell_widget__word_label_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_spell_widget_bind_addin (GbpSpellWidget          *self,
                             GbpSpellEditorPageAddin *editor_page_addin,
                             DzlSignalGroup          *editor_page_addin_signals)
{
  GspellChecker *checker;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (GBP_IS_SPELL_EDITOR_PAGE_ADDIN (editor_page_addin));
  g_assert (DZL_IS_SIGNAL_GROUP (editor_page_addin_signals));
  g_assert (self->editor_page_addin == NULL);

  self->editor_page_addin = g_object_ref (editor_page_addin);

  gbp_spell_editor_page_addin_begin_checking (editor_page_addin);

  checker = gbp_spell_editor_page_addin_get_checker (editor_page_addin);
  gbp_spell_dict_set_checker (self->dict, checker);

  self->language = gspell_checker_get_language (checker);
  gspell_language_chooser_set_language (GSPELL_LANGUAGE_CHOOSER (self->language_chooser_button), self->language);

  self->spellchecking_status = TRUE;

  _gbp_spell_widget_move_next_word (self);
}

static void
gbp_spell_widget_unbind_addin (GbpSpellWidget *self,
                               DzlSignalGroup *editor_page_addin_signals)
{
  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (DZL_IS_SIGNAL_GROUP (editor_page_addin_signals));

  if (self->editor_page_addin != NULL)
    {
      gbp_spell_editor_page_addin_end_checking (self->editor_page_addin);
      gbp_spell_dict_set_checker (self->dict, NULL);
      self->language = NULL;
      gspell_language_chooser_set_language (GSPELL_LANGUAGE_CHOOSER (self->language_chooser_button), NULL);

      g_clear_object (&self->editor_page_addin);

      _gbp_spell_widget_update_actions (self);
    }
}

static void
gbp_spell_widget_destroy (GtkWidget *widget)
{
  GbpSpellWidget *self = (GbpSpellWidget *)widget;

  g_assert (GBP_IS_SPELL_WIDGET (self));

  dzl_clear_source (&self->check_word_timeout_id);
  dzl_clear_source (&self->dict_check_word_timeout_id);

  if (self->editor != NULL)
    gbp_spell_widget_set_editor (self, NULL);

  self->language = NULL;

  /* Ensure reference holding things are released */
  g_clear_object (&self->editor);
  g_clear_object (&self->editor_page_addin);
  g_clear_object (&self->editor_page_addin_signals);
  g_clear_object (&self->dict);
  g_clear_pointer (&self->words_array, g_ptr_array_unref);

  GTK_WIDGET_CLASS (gbp_spell_widget_parent_class)->destroy (widget);
}

static void
gbp_spell_widget_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpSpellWidget *self = GBP_SPELL_WIDGET (object);

  switch (prop_id)
    {
    case PROP_EDITOR:
      g_value_set_object (value, gbp_spell_widget_get_editor (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_widget_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbpSpellWidget *self = GBP_SPELL_WIDGET (object);

  switch (prop_id)
    {
    case PROP_EDITOR:
      gbp_spell_widget_set_editor (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_widget_class_init (GbpSpellWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_spell_widget_constructed;
  object_class->get_property = gbp_spell_widget_get_property;
  object_class->set_property = gbp_spell_widget_set_property;

  widget_class->destroy = gbp_spell_widget_destroy;

  properties [PROP_EDITOR] =
    g_param_spec_object ("editor", NULL, NULL,
                         IDE_TYPE_EDITOR_PAGE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/spellcheck/gbp-spell-widget.ui");

  gtk_widget_class_bind_template_child (widget_class, GbpSpellWidget, word_label);
  gtk_widget_class_bind_template_child (widget_class, GbpSpellWidget, count_label);
  gtk_widget_class_bind_template_child (widget_class, GbpSpellWidget, word_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpSpellWidget, language_chooser_button);
  gtk_widget_class_bind_template_child (widget_class, GbpSpellWidget, suggestions_box);
  gtk_widget_class_bind_template_child (widget_class, GbpSpellWidget, dict_word_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpSpellWidget, dict_add_button);
  gtk_widget_class_bind_template_child (widget_class, GbpSpellWidget, dict_words_list);
  gtk_widget_class_bind_template_child (widget_class, GbpSpellWidget, count_box);
  gtk_widget_class_bind_template_child (widget_class, GbpSpellWidget, close_button);

  g_type_ensure (GBP_TYPE_SPELL_LANGUAGE_POPOVER);
}

static void
gbp_spell_widget_init (GbpSpellWidget *self)
{
  self->dict = gbp_spell_dict_new (NULL);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_swapped (self->dict_words_list,
                            "key-press-event",
                            G_CALLBACK (dict_row_key_pressed_event_cb),
                            self);

  self->editor_page_addin_signals = dzl_signal_group_new (GBP_TYPE_SPELL_EDITOR_PAGE_ADDIN);

  g_signal_connect_swapped (self->editor_page_addin_signals,
                            "bind",
                            G_CALLBACK (gbp_spell_widget_bind_addin),
                            self);

  g_signal_connect_swapped (self->editor_page_addin_signals,
                            "unbind",
                            G_CALLBACK (gbp_spell_widget_unbind_addin),
                            self);
}

/**
 * gbp_spell_widget_get_editor:
 * @self: a #GbpSpellWidget
 *
 * Gets the editor that is currently being spellchecked.
 *
 * Returns: (nullable) (transfer none): An #IdeEditorPage or %NULL
 *
 * Since: 3.26
 */
IdeEditorPage *
gbp_spell_widget_get_editor (GbpSpellWidget *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_WIDGET (self), NULL);

  return self->editor;
}

void
gbp_spell_widget_set_editor (GbpSpellWidget *self,
                             IdeEditorPage  *editor)
{
  GspellNavigator *navigator;

  g_return_if_fail (GBP_IS_SPELL_WIDGET (self));
  g_return_if_fail (!editor || IDE_IS_EDITOR_PAGE (editor));

  if (g_set_object (&self->editor, editor))
    {
      IdeEditorPageAddin *addin = NULL;

      if (editor != NULL)
        {
          addin = ide_editor_page_addin_find_by_module_name (editor, "spellcheck");
          navigator = gbp_spell_editor_page_addin_get_navigator (GBP_SPELL_EDITOR_PAGE_ADDIN (addin));
          g_signal_connect_object (navigator,
                                   "notify::words-counted",
                                   G_CALLBACK (gbp_spell_widget__words_counted_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
        }

      dzl_signal_group_set_target (self->editor_page_addin_signals, addin);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EDITOR]);
    }
}

GtkWidget *
gbp_spell_widget_new (IdeEditorPage *editor)
{
  g_return_val_if_fail (!editor || IDE_IS_EDITOR_PAGE (editor), NULL);

  return g_object_new (GBP_TYPE_SPELL_WIDGET,
                       "editor", editor,
                       NULL);
}

void
_gbp_spell_widget_change (GbpSpellWidget *self,
                          gboolean        change_all)
{
  g_autofree gchar *change_to = NULL;
  GspellNavigator *navigator;
  GspellChecker *checker;
  const gchar *word;

  g_assert (GBP_IS_SPELL_WIDGET (self));
  g_assert (IDE_IS_EDITOR_PAGE (self->editor));
  g_assert (GBP_IS_SPELL_EDITOR_PAGE_ADDIN (self->editor_page_addin));

  checker = gbp_spell_editor_page_addin_get_checker (self->editor_page_addin);
  g_assert (GSPELL_IS_CHECKER (checker));

  word = gtk_label_get_text (self->word_label);
  g_assert (!dzl_str_empty0 (word));

  change_to = g_strdup (gtk_entry_get_text (self->word_entry));
  g_assert (!dzl_str_empty0 (change_to));

  navigator = gbp_spell_editor_page_addin_get_navigator (self->editor_page_addin);
  g_assert (navigator != NULL);

  gspell_checker_set_correction (checker, word, -1, change_to, -1);

  if (change_all)
    gspell_navigator_change_all (navigator, word, change_to);
  else
    gspell_navigator_change (navigator, word, change_to);

  _gbp_spell_widget_move_next_word (self);
}

