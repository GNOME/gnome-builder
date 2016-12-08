/* ide-editor-spell-widget.c
 *
 * Copyright (C) 2016 Sebastien Lafargue <slafargue@gnome.org>
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
 */
#include "ide.h"

#include <glib/gi18n.h>
#include <gspell/gspell.h>

#include "ide-editor-spell-widget.h"

struct _IdeEditorSpellWidget
{
  GtkBin                 parent_instance;

  GspellNavigator       *navigator;
  IdeSourceView         *view;
  IdeBuffer             *buffer;
  GspellChecker         *checker;
  const GspellLanguage  *spellchecker_language;

  GtkLabel              *word_label;
  GtkEntry              *word_entry;
  GtkButton             *check_button;
  GtkButton             *add_dict_button;
  GtkButton             *ignore_button;
  GtkButton             *ignore_all_button;
  GtkButton             *change_button;
  GtkButton             *change_all_button;
  GtkButton             *close_button;
  GtkListBox            *suggestions_box;

  GtkButton             *highlight_checkbutton;
  GtkButton             *language_chooser_button;

  GtkWidget             *placeholder;
  GAction               *view_spellchecking_action;

  guint                  view_spellchecker_set : 1;
};

G_DEFINE_TYPE (IdeEditorSpellWidget, ide_editor_spell_widget, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_VIEW,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
clear_suggestions_box (IdeEditorSpellWidget *self)
{
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (self->suggestions_box));

  for (GList *l = children; l != NULL; l = g_list_next (l))
    gtk_widget_destroy (GTK_WIDGET (l->data));
}

static void
set_sensiblility (IdeEditorSpellWidget *self,
                  gboolean              sensibility)
{
  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));

  gtk_entry_set_text (self->word_entry, "");
  clear_suggestions_box (self);

  gtk_widget_set_sensitive (GTK_WIDGET (self->word_entry), sensibility);
  gtk_widget_set_sensitive (GTK_WIDGET (self->check_button), sensibility);
  gtk_widget_set_sensitive (GTK_WIDGET (self->ignore_button), sensibility);
  gtk_widget_set_sensitive (GTK_WIDGET (self->ignore_all_button), sensibility);
  gtk_widget_set_sensitive (GTK_WIDGET (self->change_button), sensibility);
  gtk_widget_set_sensitive (GTK_WIDGET (self->change_all_button), sensibility);
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_dict_button), sensibility);
  gtk_widget_set_sensitive (GTK_WIDGET (self->suggestions_box), sensibility);
}

GtkWidget *
ide_editor_spell_widget_get_entry (IdeEditorSpellWidget *self)
{
  return GTK_WIDGET (self->word_entry);
}

static GtkWidget *
create_suggestion_row (IdeEditorSpellWidget *self,
                       const gchar          *word)
{
  GtkWidget *row;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));

  row = g_object_new (GTK_TYPE_LABEL,
                      "label", word,
                      "visible", TRUE,
                      "halign", GTK_ALIGN_START,
                      NULL);

  return row;
}

static void
fill_suggestions_box (IdeEditorSpellWidget *self,
                      const gchar          *word,
                      gchar               **first_result)
{
  GSList *suggestions = NULL;
  GtkWidget *item;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));

  clear_suggestions_box (self);
  if (ide_str_empty0 (word))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->suggestions_box), FALSE);
      return;
    }

  if (NULL == (suggestions = gspell_checker_get_suggestions (self->checker, word, -1)))
    {
      *first_result = NULL;
      gtk_widget_set_sensitive (GTK_WIDGET (self->suggestions_box), FALSE);
    }
  else
    {
      *first_result = g_strdup (suggestions->data);
      gtk_widget_set_sensitive (GTK_WIDGET (self->suggestions_box), TRUE);
      for (GSList *l = (GSList *)suggestions; l != NULL; l = l->next)
        {
          item = create_suggestion_row (self, l->data);
          gtk_list_box_insert (self->suggestions_box, item, -1);
        }

      g_slist_free_full (suggestions, g_free);
    }
}

static gboolean
jump_to_next_misspelled_word (IdeEditorSpellWidget *self)
{
  GspellChecker *checker = NULL;
  g_autofree gchar *word = NULL;
  g_autofree gchar *first_result = NULL;
  GtkListBoxRow *row;
  GError *error = NULL;
  gboolean ret = FALSE;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->word_entry));
  if ((ret = gspell_navigator_goto_next (self->navigator, &word, &checker, &error)))
    {
      gtk_label_set_text (self->word_label, word);
      fill_suggestions_box (self, word, &first_result);
      if (!ide_str_empty0 (first_result))
        {
          row = gtk_list_box_get_row_at_index (self->suggestions_box, 0);
          gtk_list_box_select_row (self->suggestions_box, row);
        }
    }
  else
    {
      if (error != NULL)
        gtk_label_set_text (GTK_LABEL (self->placeholder), error->message);
      else
        {
          gtk_label_set_text (GTK_LABEL (self->placeholder), _("Completed spell checking"));
          set_sensiblility (self, FALSE);
        }
    }

  return ret;
}

GtkWidget *
ide_editor_spell_widget_new (IdeSourceView *source_view)
{
  return g_object_new (IDE_TYPE_EDITOR_SPELL_WIDGET,
                       "view", source_view,
                       NULL);
}

static IdeSourceView *
ide_editor_spell_widget_get_view (IdeEditorSpellWidget *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SPELL_WIDGET (self), NULL);

  return self->view;
}

static void
ide_editor_spell_widget_set_view (IdeEditorSpellWidget *self,
                                  IdeSourceView        *view)
{
  g_return_if_fail (IDE_IS_EDITOR_SPELL_WIDGET (self));
  g_return_if_fail (IDE_IS_SOURCE_VIEW (view));

  self->view = view;
  if (GSPELL_IS_NAVIGATOR (self->navigator))
    g_clear_object (&self->navigator);

  self->navigator = gspell_navigator_text_view_new (GTK_TEXT_VIEW (view));
}

static void
ide_editor_spell_widget__word_entry_changed_cb (IdeEditorSpellWidget *self,
                                                GtkEntry             *entry)
{
  gboolean sensitive;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));
  g_assert (GTK_IS_ENTRY (entry));

  sensitive = (gtk_entry_get_text_length (entry) > 0);

  gtk_widget_set_sensitive (GTK_WIDGET (self->check_button), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->change_button), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->change_all_button), sensitive);
}

static void
ide_editor_spell_widget__check_button_clicked_cb (IdeEditorSpellWidget *self,
                                                  GtkButton            *button)
{
  const gchar *word;
  g_autofree gchar *first_result = NULL;
  GError *error = NULL;
  gboolean ret = FALSE;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  word = gtk_entry_get_text (self->word_entry);
  g_assert (!ide_str_empty0 (word));

  ret = gspell_checker_check_word (self->checker, word, -1, &error);
  if (error != NULL)
    {
      printf ("check error:%s\n", error->message);
    }

  if (ret)
    {
      gtk_label_set_text (GTK_LABEL (self->placeholder), _("Correct spelling"));
      fill_suggestions_box (self, NULL, NULL);
    }
  else
    {
      fill_suggestions_box (self, word, &first_result);
      if (!ide_str_empty0 (first_result))
        gtk_entry_set_text (self->word_entry, first_result);
      else
        gtk_label_set_text (GTK_LABEL (self->placeholder), _("No suggestioons"));
    }
}

static void
ide_editor_spell_widget__add_dict_button_clicked_cb (IdeEditorSpellWidget *self,
                                                     GtkButton            *button)
{
  const gchar *word;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  word = gtk_label_get_text (self->word_label);
  g_assert (!ide_str_empty0 (word));

  gspell_checker_add_word_to_personal (self->checker, word, -1);
  jump_to_next_misspelled_word (self);
}

static void
ide_editor_spell_widget__ignore_button_clicked_cb (IdeEditorSpellWidget *self,
                                                   GtkButton            *button)
{
  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  jump_to_next_misspelled_word (self);
}

static void
ide_editor_spell_widget__ignore_all_button_clicked_cb (IdeEditorSpellWidget *self,
                                                       GtkButton            *button)
{
  const gchar *word;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  word = gtk_label_get_text (self->word_label);
  g_assert (!ide_str_empty0 (word));

  gspell_checker_add_word_to_session (self->checker, word, -1);
  jump_to_next_misspelled_word (self);
}

static void
change_misspelled_word (IdeEditorSpellWidget *self,
                        gboolean              change_all)
{
  const gchar *word;
  g_autofree gchar *change_to = NULL;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));

  word = gtk_label_get_text (self->word_label);
  g_assert (!ide_str_empty0 (word));

  change_to = g_strdup (gtk_entry_get_text (self->word_entry));
  g_assert (!ide_str_empty0 (change_to));

  gspell_checker_set_correction (self->checker, word, -1, change_to, -1);

  if (change_all)
    gspell_navigator_change_all (self->navigator, word, change_to);
  else
    gspell_navigator_change (self->navigator, word, change_to);

  jump_to_next_misspelled_word (self);
}

static void
ide_editor_spell_widget__change_button_clicked_cb (IdeEditorSpellWidget *self,
                                                   GtkButton            *button)
{
  change_misspelled_word (self, FALSE);
}

static void
ide_editor_spell_widget__change_all_button_clicked_cb (IdeEditorSpellWidget *self,
                                                       GtkButton            *button)
{
  change_misspelled_word (self, TRUE);
}

static void
ide_editor_spell_widget__row_selected_cb (IdeEditorSpellWidget *self,
                                          GtkListBoxRow        *row,
                                          GtkListBox           *listbox)
{
  const gchar *word;
  GtkLabel *label;

  if (row != NULL)
    {
      label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (row)));
      word = gtk_label_get_text (label);
      gtk_entry_set_text (self->word_entry, word);
      gtk_editable_set_position (GTK_EDITABLE (self->word_entry), -1);
    }
}

static void
ide_editor_spell_widget__row_activated_cb (IdeEditorSpellWidget *self,
                                           GtkListBoxRow        *row,
                                           GtkListBox           *listbox)
{
  if (row != NULL)
    change_misspelled_word (self, FALSE);
}

static gboolean
ide_editor_spell_widget__key_press_event_cb (IdeEditorSpellWidget *self,
                                             GdkEventKey          *event)
{
  g_assert (IDE_IS_SOURCE_VIEW (self->view));

  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      ide_widget_action (GTK_WIDGET (self), "spell-entry", "exit-spell", NULL);
      return GDK_EVENT_STOP;

    default:
      break;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_editor_frame_spell_widget_mapped_cb (IdeEditorSpellWidget *self)
{
  GActionGroup *group = NULL;
  GtkWidget *widget = GTK_WIDGET (self);
  g_autoptr (GVariant) value = NULL;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));

  while ((group == NULL) && (widget != NULL))
    {
      group = gtk_widget_get_action_group (widget, "view");
      widget = gtk_widget_get_parent (widget);
    }

  if (group != NULL &&
      NULL != (self->view_spellchecking_action = g_action_map_lookup_action (G_ACTION_MAP (group),
                                                                             "spellchecking")))
    {
      value = g_action_get_state (self->view_spellchecking_action);
      self->view_spellchecker_set = g_variant_get_boolean (value);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->highlight_checkbutton),
                                    self->view_spellchecker_set);
    }

  jump_to_next_misspelled_word (self);
}

static void
ide_editor_spell_widget__highlight_checkbutton_toggled_cb (IdeEditorSpellWidget *self,
                                                           GtkCheckButton       *button)
{
  GspellTextView *spell_text_view;
  gboolean state;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));

  state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
  spell_text_view = gspell_text_view_get_from_gtk_text_view (GTK_TEXT_VIEW (self->view));
  gspell_text_view_set_inline_spell_checking (spell_text_view, state);
}

static void
ide_editor_spell_widget__language_notify_cb (IdeEditorSpellWidget *self,
                                             GParamSpec           *pspec,
                                             GtkButton            *language_chooser_button)
{
  const GspellLanguage *current_language;
  const GspellLanguage *spell_language;
  g_autofree gchar *word = NULL;
  g_autofree gchar *first_result = NULL;
  GtkListBoxRow *row;

  g_assert (IDE_IS_EDITOR_SPELL_WIDGET (self));

  current_language = gspell_checker_get_language (self->checker);
  spell_language = gspell_language_chooser_get_language (GSPELL_LANGUAGE_CHOOSER (language_chooser_button));
  if (gspell_language_compare (current_language, spell_language) != 0)
    {
      gspell_checker_set_language (self->checker, spell_language);
      fill_suggestions_box (self, word, &first_result);
      if (!ide_str_empty0 (first_result))
        {
          row = gtk_list_box_get_row_at_index (self->suggestions_box, 0);
          gtk_list_box_select_row (self->suggestions_box, row);
        }
    }
}

static void
ide_editor_spell_widget_constructed (GObject *object)
{
  IdeEditorSpellWidget *self = (IdeEditorSpellWidget *)object;
  GspellTextBuffer *spell_buffer;

  g_assert (IDE_IS_SOURCE_VIEW (self->view));

  self->buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view)));
  ide_buffer_set_spell_checking (self->buffer, TRUE);

  spell_buffer = gspell_text_buffer_get_from_gtk_text_buffer (GTK_TEXT_BUFFER (self->buffer));
  self->checker = gspell_text_buffer_get_spell_checker (spell_buffer);

  self->spellchecker_language = gspell_checker_get_language (self->checker);
  gspell_language_chooser_set_language (GSPELL_LANGUAGE_CHOOSER (self->language_chooser_button),
                                        self->spellchecker_language);

  self->navigator = gspell_navigator_text_view_new (GTK_TEXT_VIEW (self->view));

  g_signal_connect_swapped (self->word_entry,
                            "changed",
                            G_CALLBACK (ide_editor_spell_widget__word_entry_changed_cb),
                            self);

  g_signal_connect_swapped (self->check_button,
                            "clicked",
                            G_CALLBACK (ide_editor_spell_widget__check_button_clicked_cb),
                            self);

  g_signal_connect_swapped (self->add_dict_button,
                            "clicked",
                            G_CALLBACK (ide_editor_spell_widget__add_dict_button_clicked_cb),
                            self);

  g_signal_connect_swapped (self->ignore_button,
                            "clicked",
                            G_CALLBACK (ide_editor_spell_widget__ignore_button_clicked_cb),
                            self);

  g_signal_connect_swapped (self->ignore_all_button,
                            "clicked",
                            G_CALLBACK (ide_editor_spell_widget__ignore_all_button_clicked_cb),
                            self);

  g_signal_connect_swapped (self->change_button,
                            "clicked",
                            G_CALLBACK (ide_editor_spell_widget__change_button_clicked_cb),
                            self);

  g_signal_connect_swapped (self->change_all_button,
                            "clicked",
                            G_CALLBACK (ide_editor_spell_widget__change_all_button_clicked_cb),
                            self);

  g_signal_connect_swapped (self->suggestions_box,
                            "row-selected",
                            G_CALLBACK (ide_editor_spell_widget__row_selected_cb),
                            self);

  g_signal_connect_swapped (self->suggestions_box,
                            "row-activated",
                            G_CALLBACK (ide_editor_spell_widget__row_activated_cb),
                            self);

  g_signal_connect_swapped (self,
                            "key-press-event",
                            G_CALLBACK (ide_editor_spell_widget__key_press_event_cb),
                            self);

  g_signal_connect_swapped (self->highlight_checkbutton,
                            "toggled",
                            G_CALLBACK (ide_editor_spell_widget__highlight_checkbutton_toggled_cb),
                            self);

  g_signal_connect_object (self->language_chooser_button,
                           "notify::language",
                           G_CALLBACK (ide_editor_spell_widget__language_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->placeholder = gtk_label_new (NULL);
  gtk_widget_set_visible (self->placeholder, TRUE);
  gtk_list_box_set_placeholder (self->suggestions_box, self->placeholder);

  /* Due to the change of focus between the view and the spellchecker widget,
   * we need to start checking only when the widget is mapped,
   * so the view can keep the selection on the first word.
   */
  g_signal_connect_object (self,
                           "map",
                           G_CALLBACK (ide_editor_frame_spell_widget_mapped_cb),
                           NULL,
                           G_CONNECT_AFTER);
}

static void
ide_editor_spell_widget_finalize (GObject *object)
{
  IdeEditorSpellWidget *self = (IdeEditorSpellWidget *)object;
  GspellTextView *spell_text_view;
  const GspellLanguage *spell_language;
  GtkTextBuffer *buffer;

  printf ("ide_editor_spell_widget_finalize\n");
  g_clear_object (&self->navigator);

  /* Set back the view spellchecking previous state */
  spell_text_view = gspell_text_view_get_from_gtk_text_view (GTK_TEXT_VIEW (self->view));
  if (self->view_spellchecker_set)
    {
      gspell_text_view_set_inline_spell_checking (spell_text_view, TRUE);
      spell_language = gspell_checker_get_language (self->checker);
      if (gspell_language_compare (self->spellchecker_language, spell_language) != 0)
        gspell_checker_set_language (self->checker, self->spellchecker_language);
    }
  else
    {
      gspell_text_view_set_inline_spell_checking (spell_text_view, FALSE);
      gspell_text_view_set_enable_language_menu (spell_text_view, FALSE);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
      ide_buffer_set_spell_checking (IDE_BUFFER (buffer), FALSE);
    }

  G_OBJECT_CLASS (ide_editor_spell_widget_parent_class)->finalize (object);
}

static void
ide_editor_spell_widget_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeEditorSpellWidget *self = IDE_EDITOR_SPELL_WIDGET (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, ide_editor_spell_widget_get_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_spell_widget_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeEditorSpellWidget *self = IDE_EDITOR_SPELL_WIDGET (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      ide_editor_spell_widget_set_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_spell_widget_class_init (IdeEditorSpellWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_editor_spell_widget_constructed;
  object_class->finalize = ide_editor_spell_widget_finalize;
  object_class->get_property = ide_editor_spell_widget_get_property;
  object_class->set_property = ide_editor_spell_widget_set_property;

  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The source view.",
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-spell-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, word_label);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, word_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, check_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, add_dict_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, ignore_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, ignore_all_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, change_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, change_all_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, close_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, highlight_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, language_chooser_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSpellWidget, suggestions_box);
}

static void
ide_editor_spell_widget_init (IdeEditorSpellWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->view_spellchecker_set = FALSE;
}
