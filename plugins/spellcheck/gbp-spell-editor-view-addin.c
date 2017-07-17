/* gbp-spell-editor-view-addin.c
 *
 * Copyright (C) 2016 Sebastien Lafargue <slafargue@gnome.org>
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-spell-editor-view-addin"

#include <gspell/gspell.h>
#include <glib/gi18n.h>

#include "gbp-spell-buffer-addin.h"
#include "gbp-spell-editor-view-addin.h"
#include "gbp-spell-utils.h"

#define SPELLCHECKER_SUBREGION_LENGTH 500

struct _GbpSpellEditorViewAddin
{
  GObject          parent_instance;

  /* Borrowed references */
  IdeEditorView   *view;
  GtkTextMark     *word_begin;
  GtkTextMark     *word_end;
  GtkTextMark     *start_boundary;
  GtkTextMark     *end_boundary;

  /* Owned references */
  DzlBindingGroup *buffer_addin_bindings;

  gint             checking_count;
};

enum {
  COMPLETED,
  FAILED,
  WORD_CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static GtkTextTag *
get_misspelled_tag (GbpSpellEditorViewAddin *self)
{
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (self->buffer_addin_bindings != NULL)
    {
      GObject *addin;

      addin = dzl_binding_group_get_source (self->buffer_addin_bindings);
      if (addin != NULL)
        return gbp_spell_buffer_addin_get_misspelled_tag (GBP_SPELL_BUFFER_ADDIN (addin));
    }

  return NULL;
}

static void
gbp_spell_editor_view_addin_load (IdeEditorViewAddin *addin,
                                  IdeEditorView      *view)
{
  GbpSpellEditorViewAddin *self = (GbpSpellEditorViewAddin *)addin;
  g_autoptr(DzlPropertiesGroup) group = NULL;
  IdeBufferAddin *buffer_addin;
  GspellTextView *wrapper;
  IdeSourceView *source_view;
  IdeBuffer *buffer;

  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  self->view = view;

  source_view = ide_editor_view_get_view (view);
  g_assert (source_view != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  buffer = ide_editor_view_get_buffer (view);
  g_assert (buffer != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  buffer_addin = ide_buffer_addin_find_by_module_name (buffer, "spellcheck-plugin");
  g_assert (buffer_addin != NULL);
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (buffer_addin));

  wrapper = gspell_text_view_get_from_gtk_text_view (GTK_TEXT_VIEW (source_view));
  g_assert (wrapper != NULL);
  g_assert (GSPELL_IS_TEXT_VIEW (wrapper));

  self->buffer_addin_bindings = dzl_binding_group_new ();
  dzl_binding_group_bind (self->buffer_addin_bindings, "enabled",
                          wrapper, "enable-language-menu",
                          G_BINDING_SYNC_CREATE);
  dzl_binding_group_bind (self->buffer_addin_bindings, "enabled",
                          wrapper, "inline-spell-checking",
                          G_BINDING_SYNC_CREATE);
  dzl_binding_group_set_source (self->buffer_addin_bindings, buffer_addin);

  group = dzl_properties_group_new (G_OBJECT (buffer_addin));
  dzl_properties_group_add_all_properties (group);
  gtk_widget_insert_action_group (GTK_WIDGET (view), "spellcheck", G_ACTION_GROUP (group));
}

static void
gbp_spell_editor_view_addin_unload (IdeEditorViewAddin *addin,
                                    IdeEditorView      *view)
{
  GbpSpellEditorViewAddin *self = (GbpSpellEditorViewAddin *)addin;

  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  gtk_widget_insert_action_group (GTK_WIDGET (view), "spellcheck", NULL);

  dzl_binding_group_set_source (self->buffer_addin_bindings, NULL);
  g_clear_object (&self->buffer_addin_bindings);

  self->view = NULL;
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gbp_spell_editor_view_addin_load;
  iface->unload = gbp_spell_editor_view_addin_unload;
}

static void
gbp_spell_editor_view_addin_select_misspelled_word (GbpSpellEditorViewAddin *self)
{
  IdeSourceView *view;
  GtkTextBuffer *buffer;
  GtkTextTag *tag;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));

  if (self->view == NULL)
    return;

  view = ide_editor_view_get_view (self->view);
  buffer = GTK_TEXT_BUFFER (ide_editor_view_get_buffer (self->view));
  tag = get_misspelled_tag (self);

  if (buffer != NULL && tag != NULL)
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &begin, self->start_boundary);
      gtk_text_buffer_get_iter_at_mark (buffer, &end, self->end_boundary);
      gtk_text_buffer_remove_tag (buffer, tag, &begin, &end);

      gtk_text_buffer_get_iter_at_mark (buffer, &begin, self->word_begin);
      gtk_text_buffer_get_iter_at_mark (buffer, &end, self->word_end);
      gtk_text_buffer_apply_tag (buffer, tag, &begin, &end);

      gtk_widget_queue_draw (GTK_WIDGET (view));

      ide_source_view_scroll_to_mark (view, self->word_begin, 0.25, TRUE, 1.0, 0.0, TRUE);
    }
}

static gboolean
gbp_spell_editor_view_addin_goto_next (GspellNavigator  *navigator,
                                       gchar           **word,
                                       GspellChecker   **checker,
                                       GError          **error)
{
  GbpSpellEditorViewAddin *self = (GbpSpellEditorViewAddin *)navigator;
  g_autofree gchar *real_word = NULL;
  GspellChecker *spell_checker;
  GtkTextBuffer *buffer;
  GtkTextIter word_begin;
  GtkTextIter word_end;
  GtkTextIter end;
  GtkTextTag *no_spell_check_tag;
  gboolean ret = FALSE;

  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));

  if (self->view == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVAL,
                   "Cannot run spellchecker without view");
      goto complete;
    }

  if (self->checking_count < 1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVAL,
                   "Cannot run spellchecker before gbp_spell_editor_view_addin_begin_checking()");
      goto complete;
    }

  g_assert (self->word_begin != NULL);
  g_assert (self->word_end != NULL);
  g_assert (self->start_boundary != NULL);
  g_assert (self->end_boundary != NULL);

  buffer = GTK_TEXT_BUFFER (ide_editor_view_get_buffer (self->view));
  spell_checker = gbp_spell_editor_view_addin_get_checker (self);

  if (spell_checker == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVAL,
                   "Cannot run spellchecker without buffer");
      goto complete;
    }

  if (gspell_checker_get_language (spell_checker) == NULL)
    {
      g_set_error (error,
                   GSPELL_CHECKER_ERROR,
                   GSPELL_CHECKER_ERROR_NO_LANGUAGE_SET,
                   "%s",
                   _("No language set. Check your dictionary installation."));
      goto complete;
    }

  gtk_text_buffer_get_iter_at_mark (buffer, &end, self->end_boundary);
  gtk_text_buffer_get_iter_at_mark (buffer, &word_end, self->word_end);

  if (gtk_text_iter_compare (&end, &word_end) <= 0)
    goto complete;

  word_begin = word_end;
  no_spell_check_tag = gbp_spell_utils_get_no_spell_check_tag (buffer);

  while (TRUE)
    {
      g_autofree gchar *local_word = NULL;
      g_autoptr(GError) local_error = NULL;
      gboolean correctly_spelled;

      if (!gbp_spell_utils_text_iter_starts_word (&word_begin))
        {
          GtkTextIter iter;

          iter = word_begin;
          gbp_spell_utils_text_iter_forward_word_end (&word_begin);

          if (gtk_text_iter_equal (&iter, &word_begin))
            goto complete;

          gbp_spell_utils_text_iter_backward_word_start (&word_begin);
        }

      if (!gbp_spell_utils_skip_no_spell_check (no_spell_check_tag, &word_begin, &end))
        goto complete;

      g_return_val_if_fail (gbp_spell_utils_text_iter_starts_word (&word_begin), FALSE);

      word_end = word_begin;
      gbp_spell_utils_text_iter_forward_word_end (&word_end);

      if (gtk_text_iter_compare (&end, &word_end) < 0)
        goto complete;

      local_word = gtk_text_buffer_get_text (buffer, &word_begin, &word_end, FALSE);
      correctly_spelled = gspell_checker_check_word (spell_checker, local_word, -1, &local_error);

      if (local_error != NULL)
        {
          g_propagate_error (error, g_steal_pointer (&local_error));
          goto complete;
        }

      if (!correctly_spelled)
        {
          /* Found! */
          gtk_text_buffer_move_mark (buffer, self->word_begin, &word_begin);
          gtk_text_buffer_move_mark (buffer, self->word_end, &word_end);

          gbp_spell_editor_view_addin_select_misspelled_word (self);

          real_word = g_steal_pointer (&local_word);
          ret = TRUE;
          goto complete;
       }

      word_begin = word_end;
    }

complete:

  if (ret)
    {
      if (word != NULL)
        *word = g_steal_pointer (&real_word);

      if (checker != NULL)
        *checker = g_object_ref (spell_checker);
    }

  return ret;
}

static void
gbp_spell_editor_view_addin_change (GspellNavigator *navigator,
                                    const gchar     *word,
                                    const gchar     *change_to)
{
  GbpSpellEditorViewAddin *self = (GbpSpellEditorViewAddin *)navigator;
  g_autofree gchar *word_in_buffer = NULL;
  GtkTextBuffer *buffer;
  GtkTextIter word_begin;
  GtkTextIter word_end;

  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (navigator));
  g_assert (word != NULL);
  g_assert (change_to != NULL);
  g_assert (GTK_IS_TEXT_MARK (self->word_begin));
  g_assert (GTK_IS_TEXT_MARK (self->word_end));

  if (self->view == NULL)
    return;

  buffer = GTK_TEXT_BUFFER (ide_editor_view_get_buffer (self->view));

  gtk_text_buffer_get_iter_at_mark (buffer, &word_begin, self->word_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &word_end, self->word_end);

  word_in_buffer = gtk_text_buffer_get_slice (buffer, &word_begin, &word_end, TRUE);
  g_return_if_fail (word_in_buffer != NULL);
  g_return_if_fail (g_str_equal (word_in_buffer, word));

  gtk_text_buffer_begin_user_action (buffer);

  gtk_text_buffer_delete (buffer, &word_begin, &word_end);
  gtk_text_buffer_insert (buffer, &word_begin, change_to, -1);

  gtk_text_buffer_end_user_action (buffer);
}

static void
gbp_spell_editor_view_addin_change_all (GspellNavigator *navigator,
                                        const gchar     *word,
                                        const gchar     *change_to)
{
  GbpSpellEditorViewAddin *self = (GbpSpellEditorViewAddin *)navigator;
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  g_assert (GSPELL_IS_NAVIGATOR (navigator));
  g_assert (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_assert (word != NULL);
  g_assert (change_to != NULL);
  g_assert (GTK_IS_TEXT_MARK (self->start_boundary));
  g_assert (GTK_IS_TEXT_MARK (self->end_boundary));

  if (self->view == NULL)
    return;

  buffer = GTK_TEXT_BUFFER (ide_editor_view_get_buffer (self->view));

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, self->start_boundary);
  gtk_text_buffer_begin_user_action (buffer);

  while (TRUE)
    {
      gboolean found;
      GtkTextIter match_begin;
      GtkTextIter match_end;
      GtkTextIter limit;

      gtk_text_buffer_get_iter_at_mark (buffer, &limit, self->end_boundary);
      found = gtk_text_iter_forward_search (&iter,
                                            word,
                                            (GTK_TEXT_SEARCH_VISIBLE_ONLY |
                                             GTK_TEXT_SEARCH_TEXT_ONLY),
                                            &match_begin,
                                            &match_end,
                                            &limit);

      if (!found)
        break;

      if (gbp_spell_utils_text_iter_starts_word (&match_begin) &&
          gbp_spell_utils_text_iter_ends_word (&match_end))
        {
          gtk_text_buffer_delete (buffer, &match_begin, &match_end);
          gtk_text_buffer_insert (buffer, &match_end, change_to, -1);
        }

      iter = match_end;
    }

  gtk_text_buffer_end_user_action (buffer);
}

static void
navigator_iface_init (GspellNavigatorInterface *iface)
{
  iface->goto_next = gbp_spell_editor_view_addin_goto_next;
  iface->change = gbp_spell_editor_view_addin_change;
  iface->change_all = gbp_spell_editor_view_addin_change_all;
}

G_DEFINE_TYPE_WITH_CODE (GbpSpellEditorViewAddin, gbp_spell_editor_view_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GSPELL_TYPE_NAVIGATOR, navigator_iface_init)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN,
                                                editor_view_addin_iface_init))

static void
gbp_spell_editor_view_addin_class_init (GbpSpellEditorViewAddinClass *klass)
{
  /**
   * GbpSpellEditorViewAddin::completed:
   *
   * The "completed" signal is emitted after moving past the last word.
   */
  signals [COMPLETED] =
    g_signal_new ("completed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GbpSpellEditorViewAddin::failed:
   * @self: A #GbpSpellEditorViewAddin
   * @error: (in) (transfer none): A #GError containing the reason
   *
   * The "failed" signal is emitted when a failure to move to the next
   * item has occurred. @error contains the reason.
   */
  signals [FAILED] =
    g_signal_new ("completed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, G_TYPE_ERROR | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GbpSpellEditorViewAddin::word-changed:
   *
   * The "word-changed" signal is emitted when the navigator has advanced
   * to the next word. Connect to this to update your UI in reaction to
   * a change in the underlying navigator.
   */
  signals [WORD_CHANGED] =
    g_signal_new ("word-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
gbp_spell_editor_view_addin_init (GbpSpellEditorViewAddin *self)
{
}

/**
 * gbp_spell_editor_view_addin_begin_checking:
 * @self: a #GbpSpellEditorViewAddin
 *
 * This function should be called by the #GbpSpellWidget to enable
 * spellchecking on the textview and underlying buffer. Doing so allows the
 * inline-spellchecking and language-menu to be dynamically enabled even if
 * spellchecking is typically disabled in the buffer.
 *
 * The caller should call gbp_spell_editor_view_addin_end_checking() when they
 * have completed the spellchecking process.
 *
 * Since: 3.26
 */
void
gbp_spell_editor_view_addin_begin_checking (GbpSpellEditorViewAddin *self)
{
  GObject *buffer_addin;

  g_return_if_fail (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_return_if_fail (self->view != NULL);
  g_return_if_fail (self->checking_count >= 0);

  self->checking_count++;

  buffer_addin = dzl_binding_group_get_source (self->buffer_addin_bindings);

  if (buffer_addin == NULL)
    {
      g_warning ("Attempt to start spellchecking without a buffer addin");
      return;
    }

  if (self->checking_count == 1)
    {
      IdeSourceView *view;
      GtkTextBuffer *buffer;
      GtkTextIter begin;
      GtkTextIter end;

      gbp_spell_buffer_addin_begin_checking (GBP_SPELL_BUFFER_ADDIN (buffer_addin));

      view = ide_editor_view_get_view (self->view);
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

      /* Use the selected range, otherwise whole buffer */
      if (!gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
        gtk_text_buffer_get_bounds (buffer, &begin, &end);

      /* The selection might begin in the middle of a word */
      if (gbp_spell_utils_text_iter_inside_word (&begin) &&
          !gbp_spell_utils_text_iter_starts_word (&begin))
        gbp_spell_utils_text_iter_backward_word_start (&begin);

      /* And also at the end */
      if (gbp_spell_utils_text_iter_inside_word (&end))
        gbp_spell_utils_text_iter_forward_word_end (&end);

      /* Place current position at the beginning of the selection */
      self->word_begin = gtk_text_buffer_create_mark (buffer, NULL, &begin, TRUE);
      self->word_end = gtk_text_buffer_create_mark (buffer, NULL, &begin, FALSE);

      /* Setup our acceptable range for checking */
      self->start_boundary = gtk_text_buffer_create_mark (buffer, NULL, &begin, TRUE);
      self->end_boundary = gtk_text_buffer_create_mark (buffer, NULL, &end, FALSE);
    }
}

/**
 * gbp_spell_editor_view_addin_end_checking:
 * @self: a #GbpSpellEditorViewAddin
 *
 * Completes a spellcheck operation and potentially restores the buffer to
 * the visual state before spellchecking started.
 *
 * Since: 3.26
 */
void
gbp_spell_editor_view_addin_end_checking (GbpSpellEditorViewAddin *self)
{
  g_return_if_fail (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self));
  g_return_if_fail (self->checking_count >= 0);

  self->checking_count--;

  if (self->checking_count == 0)
    {
      GObject *buffer_addin;

      buffer_addin = dzl_binding_group_get_source (self->buffer_addin_bindings);

      if (GBP_IS_SPELL_BUFFER_ADDIN (buffer_addin))
        gbp_spell_buffer_addin_end_checking (GBP_SPELL_BUFFER_ADDIN (buffer_addin));


      if (self->view != NULL)
        {
          IdeSourceView *view = ide_editor_view_get_view (self->view);
          GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

          gtk_text_buffer_delete_mark (buffer, self->word_begin);
          gtk_text_buffer_delete_mark (buffer, self->word_end);
          gtk_text_buffer_delete_mark (buffer, self->start_boundary);
          gtk_text_buffer_delete_mark (buffer, self->end_boundary);
        }

      self->word_begin = NULL;
      self->word_end = NULL;
      self->start_boundary = NULL;
      self->end_boundary = NULL;
    }
}

/**
 * gbp_spell_editor_view_addin_get_checker:
 * @self: a #GbpSpellEditorViewAddin
 *
 * This function may return %NULL before
 * gbp_spell_editor_view_addin_begin_checking() has been called.
 *
 * Returns: (nullable) (transfer none): A #GspellChecker or %NULL
 *
 * Since: 3.26
 */
GspellChecker *
gbp_spell_editor_view_addin_get_checker (GbpSpellEditorViewAddin *self)
{
  GObject *buffer_addin;

  g_return_val_if_fail (GBP_IS_SPELL_EDITOR_VIEW_ADDIN (self), NULL);

  buffer_addin = dzl_binding_group_get_source (self->buffer_addin_bindings);
  if (GBP_IS_SPELL_BUFFER_ADDIN (buffer_addin))
    return gbp_spell_buffer_addin_get_checker (GBP_SPELL_BUFFER_ADDIN (buffer_addin));

  return NULL;
}
