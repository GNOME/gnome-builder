/* gbp-spell-navigator.c
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

 * This code is a modification of:
 * https://git.gnome.org/browse/gspell/tree/gspell/gspell-navigator-text-view.c
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>
#include <libide-editor.h>

#include "gbp-spell-buffer-addin.h"
#include "gbp-spell-navigator.h"
#include "gbp-spell-utils.h"

#define SPELLCHECKER_SUBREGION_LENGTH 500

struct _GbpSpellNavigator
{
  GObject          parent_instance;

  GtkTextView     *view;
  GtkTextBuffer   *buffer;

  GHashTable      *words_count;
  GtkTextMark     *start_boundary;
  GtkTextMark     *end_boundary;
  GtkTextMark     *word_start;
  GtkTextMark     *word_end;

  guint            words_counted : 1;
};

static void gspell_navigator_iface_init (GspellNavigatorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpSpellNavigator, gbp_spell_navigator, G_TYPE_INITIALLY_UNOWNED,
                         G_IMPLEMENT_INTERFACE (GSPELL_TYPE_NAVIGATOR, gspell_navigator_iface_init))

enum {
  PROP_0,
  PROP_VIEW,
  PROP_WORDS_COUNTED,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

typedef struct
{
  GbpSpellNavigator    *navigator;
  GtkSourceRegion      *words_count_region;
  GtkSourceRegionIter   iter;
} WordsCountState;

static void
words_count_state_free (gpointer user_data)
{
  WordsCountState *state = user_data;

  g_clear_object (&state->navigator);
  g_clear_object (&state->words_count_region);
  g_slice_free (WordsCountState, state);
}

static GtkTextTag *
get_misspelled_tag (GbpSpellNavigator *self)
{
  IdeBufferAddin *buffer_addin;

  g_assert (GBP_IS_SPELL_NAVIGATOR (self));
  g_assert (self->buffer != NULL);
  g_assert (IDE_IS_BUFFER (self->buffer));

  buffer_addin = ide_buffer_addin_find_by_module_name (IDE_BUFFER (self->buffer), "spellcheck");
  if (buffer_addin != NULL)
    return gbp_spell_buffer_addin_get_misspelled_tag (GBP_SPELL_BUFFER_ADDIN (buffer_addin));

  return NULL;
}

static void
gbp_spell_navigator_select_misspelled_word (GbpSpellNavigator *self)
{
  GtkTextTag *tag;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GBP_IS_SPELL_NAVIGATOR (self));

  if (self->view == NULL)
    return;

  if (self->buffer != NULL && NULL != (tag = get_misspelled_tag (self)))
    {
      gtk_text_buffer_get_iter_at_mark (self->buffer, &begin, self->start_boundary);
      gtk_text_buffer_get_iter_at_mark (self->buffer, &end, self->end_boundary);
      gtk_text_buffer_remove_tag (self->buffer, tag, &begin, &end);

      gtk_text_buffer_get_iter_at_mark (self->buffer, &begin, self->word_start);
      gtk_text_buffer_get_iter_at_mark (self->buffer, &end, self->word_end);
      gtk_text_buffer_apply_tag (self->buffer, tag, &begin, &end);

      gtk_widget_queue_draw (GTK_WIDGET (self->view));

      ide_source_view_scroll_to_mark (IDE_SOURCE_VIEW (self->view), self->word_start,
                                      0.25, TRUE, 1.0, 0.0, TRUE);
    }
}

static gboolean
gbp_spell_navigator_words_count_cb (WordsCountState *state)
{
  GbpSpellNavigator *self = state->navigator;
  GtkTextTag *no_spell_check_tag;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter word_start;
  GtkTextIter word_end;
  gchar *word;
  guint count;

  g_assert (GBP_IS_SPELL_NAVIGATOR (self));

  no_spell_check_tag = gbp_spell_utils_get_no_spell_check_tag (self->buffer);

  if (gtk_source_region_iter_get_subregion (&state->iter, &begin, &end))
    {
      word_start = word_end = begin;

      while (TRUE)
        {
          if (!gbp_spell_utils_text_iter_starts_word (&word_start))
            {
              GtkTextIter iter;

              iter = word_start;
              gbp_spell_utils_text_iter_forward_word_end (&word_start);
              if (gtk_text_iter_equal (&iter, &word_start))
                break;

              gbp_spell_utils_text_iter_backward_word_start (&word_start);
            }

          if (!gbp_spell_utils_skip_no_spell_check (no_spell_check_tag, &word_start, &end))
            break;

          word_end = word_start;
          gbp_spell_utils_text_iter_forward_word_end (&word_end);
          if (gtk_text_iter_compare (&word_end, &end) >= 0)
            break;

          word = gtk_text_buffer_get_text (self->buffer, &word_start, &word_end, FALSE);

          count = GPOINTER_TO_UINT (g_hash_table_lookup (self->words_count, word));
          count++;

          g_hash_table_insert (self->words_count, word, GUINT_TO_POINTER (count));

          word_start = word_end;
        }

      if (gtk_source_region_iter_next (&state->iter))
        return G_SOURCE_CONTINUE;
    }

  self->words_counted = TRUE;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WORDS_COUNTED]);

  return G_SOURCE_REMOVE;
}

/* Always process start and end by init_boudaries before */
static GHashTable *
gbp_spell_navigator_count_words (GbpSpellNavigator *self,
                                 GtkTextIter       *start,
                                 GtkTextIter       *end)
{
  GHashTable *table;
  GtkSourceRegion *words_count_region;
  WordsCountState *state;
  GtkTextIter start_subregion;
  GtkTextIter end_subregion;
  gint line_start;
  gint line_end;
  gint nb_subregion;

  g_assert (GBP_IS_SPELL_NAVIGATOR (self));
  g_assert (start != NULL);
  g_assert (end != NULL);

  words_count_region = gtk_source_region_new (self->buffer);
  line_start = gtk_text_iter_get_line (start);
  line_end = gtk_text_iter_get_line (end);
  nb_subregion = (line_end - line_start + 1) / SPELLCHECKER_SUBREGION_LENGTH;

  if (nb_subregion > 1)
    {
      for (gint i = 0; i < nb_subregion; ++i)
        {
          line_end = line_start + SPELLCHECKER_SUBREGION_LENGTH - 1;
          gtk_text_buffer_get_iter_at_line_offset (self->buffer, &start_subregion, line_start, 0);
          gtk_text_buffer_get_iter_at_line_offset (self->buffer, &end_subregion, line_end, 0);
          if (!gtk_text_iter_ends_line (&end_subregion))
            gtk_text_iter_forward_to_line_end (&end_subregion);

          gtk_source_region_add_subregion (words_count_region, &start_subregion, &end_subregion);
          line_start = line_end + 1;
        }
    }

  gtk_text_buffer_get_iter_at_line_offset (self->buffer, &start_subregion, line_start, 0);
  gtk_source_region_add_subregion (words_count_region, &start_subregion, end);

  table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  state = g_slice_new0 (WordsCountState);
  state->navigator = g_object_ref (self);
  state->words_count_region = words_count_region;
  gtk_source_region_get_start_region_iter (words_count_region, &state->iter);

  g_idle_add_full (G_PRIORITY_LOW,
                   (GSourceFunc)gbp_spell_navigator_words_count_cb,
                   state,
                   (GDestroyNotify)words_count_state_free);

  return table;
}

gboolean
gbp_spell_navigator_get_is_words_counted (GbpSpellNavigator *self)
{
  g_assert (GBP_IS_SPELL_NAVIGATOR (self));

  return self->words_counted;
}

guint
gbp_spell_navigator_get_count (GbpSpellNavigator *self,
                               const gchar       *word)
{
  g_assert (GBP_IS_SPELL_NAVIGATOR (self));

  if (self->words_count == NULL || dzl_str_empty0 (word))
    return 0;
  else
    return GPOINTER_TO_UINT (g_hash_table_lookup (self->words_count, word));
}

GspellNavigator *
gbp_spell_navigator_new (GtkTextView *view)
{
  return g_object_new (GBP_TYPE_SPELL_NAVIGATOR,
                       "view", view,
                       NULL);
}

static void
delete_mark (GtkTextBuffer  *buffer,
             GtkTextMark   **mark)
{
  if (mark != NULL && *mark != NULL)
    {
      GtkTextMark *m = g_steal_pointer (mark);
      gtk_text_buffer_delete_mark (buffer, m);
    }
}

static void
gbp_spell_navigator_dispose (GObject *object)
{
  GbpSpellNavigator *self = (GbpSpellNavigator *)object;

  g_clear_object (&self->view);
  g_clear_pointer (&self->words_count, g_hash_table_unref);

  if (self->buffer != NULL)
    {
      delete_mark (self->buffer, &self->start_boundary);
      delete_mark (self->buffer, &self->end_boundary);
      delete_mark (self->buffer, &self->word_start);
      delete_mark (self->buffer, &self->word_end);
      g_clear_object (&self->buffer);
    }

  G_OBJECT_CLASS (gbp_spell_navigator_parent_class)->dispose (object);
}

static void
init_boundaries (GbpSpellNavigator *self)
{
  GtkTextIter start;
  GtkTextIter end;

  g_assert (GBP_IS_SPELL_NAVIGATOR (self));
  g_assert (self->start_boundary == NULL);
  g_assert (self->end_boundary == NULL);

  if (!gtk_text_buffer_get_selection_bounds (self->buffer, &start, &end))
    gtk_text_buffer_get_bounds (self->buffer, &start, &end);

  if (gbp_spell_utils_text_iter_inside_word (&start) &&
      !gbp_spell_utils_text_iter_starts_word (&start))
    gbp_spell_utils_text_iter_backward_word_start (&start);

  if (gbp_spell_utils_text_iter_inside_word (&end))
    gbp_spell_utils_text_iter_forward_word_end (&end);

  self->start_boundary = gtk_text_buffer_create_mark (self->buffer, NULL, &start, TRUE);
  self->end_boundary = gtk_text_buffer_create_mark (self->buffer, NULL, &end, FALSE);
}

static void
set_view (GbpSpellNavigator *self,
          GtkTextView       *view)
{
  GtkTextIter start;
  GtkTextIter end;

  g_assert (GBP_IS_SPELL_NAVIGATOR (self));
  g_assert (self->view == NULL);
  g_assert (self->buffer == NULL);

  if (view != self->view)
    {
      self->view = g_object_ref (view);
      self->buffer = g_object_ref (gtk_text_view_get_buffer (view));

      init_boundaries (self);

      gtk_text_buffer_get_iter_at_mark (self->buffer, &start, self->start_boundary);
      gtk_text_buffer_get_iter_at_mark (self->buffer, &end, self->end_boundary);
      self->words_count = gbp_spell_navigator_count_words (self, &start, &end);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VIEW]);
    }
}

static void
gbp_spell_navigator_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpSpellNavigator *self = GBP_SPELL_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      set_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_navigator_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpSpellNavigator *self = GBP_SPELL_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, self->view);
      break;

    case PROP_WORDS_COUNTED:
      g_value_set_boolean (value, self->words_counted);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_navigator_class_init (GbpSpellNavigatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_spell_navigator_dispose;
  object_class->get_property = gbp_spell_navigator_get_property;
  object_class->set_property = gbp_spell_navigator_set_property;

  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "the view",
                         GTK_TYPE_TEXT_VIEW,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  properties [PROP_WORDS_COUNTED] =
    g_param_spec_boolean ("words-counted",
                          "words-counted",
                          "words-counted",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_spell_navigator_init (GbpSpellNavigator *self)
{
}

/* Go to the start of the current checked word so that
 * we can re-check it again, change of language for example
 */
gboolean
gbp_spell_navigator_goto_word_start (GbpSpellNavigator *self)
{
  GtkTextIter start;

  g_assert (GBP_IS_SPELL_NAVIGATOR (self));

  if (self->word_start != NULL)
    {
      gtk_text_buffer_get_iter_at_mark (self->buffer, &start, self->word_start);
      gtk_text_buffer_move_mark (self->buffer, self->word_end, &start);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gbp_spell_navigator_goto_next (GspellNavigator  *navigator,
                               gchar           **word_p,
                               GspellChecker   **spell_checker_p,
                               GError          **error_p)
{
  GbpSpellNavigator *self = (GbpSpellNavigator *)navigator;
  GspellTextBuffer *gspell_buffer;
  GspellChecker *spell_checker;
  GtkTextIter word_start;
  GtkTextIter end;
  GtkTextTag *no_spell_check_tag;

  g_assert (GBP_IS_SPELL_NAVIGATOR (self));
  g_assert ((self->word_start == NULL && self->word_end == NULL) ||
            (self->word_start != NULL && self->word_end != NULL));

  gspell_buffer = gspell_text_buffer_get_from_gtk_text_buffer (self->buffer);
  spell_checker = gspell_text_buffer_get_spell_checker (gspell_buffer);

  if (spell_checker == NULL)
    return FALSE;

  if (gspell_checker_get_language (spell_checker) == NULL)
    {
      if (spell_checker_p != NULL)
        *spell_checker_p = g_object_ref (spell_checker);

      g_set_error (error_p,
                   GSPELL_CHECKER_ERROR,
                   GSPELL_CHECKER_ERROR_NO_LANGUAGE_SET,
                   "%s",
                   _("No language set. Check your dictionary installation."));

      return FALSE;
    }

  gtk_text_buffer_get_iter_at_mark (self->buffer, &end, self->end_boundary);

  if (self->word_start == NULL)
    {
      GtkTextIter start;

      gtk_text_buffer_get_iter_at_mark (self->buffer, &start, self->start_boundary);

      self->word_start = gtk_text_buffer_create_mark (self->buffer, NULL, &start, TRUE);
      self->word_end = gtk_text_buffer_create_mark (self->buffer, NULL, &start, FALSE);

      word_start = start;
    }
  else
    {
      GtkTextIter word_end;

      gtk_text_buffer_get_iter_at_mark (self->buffer, &word_end, self->word_end);

      if (gtk_text_iter_compare (&end, &word_end) <= 0)
        return FALSE;

      word_start = word_end;
    }

  no_spell_check_tag = gbp_spell_utils_get_no_spell_check_tag (self->buffer);

  while (TRUE)
    {
      GtkTextIter word_end;
      g_autofree gchar *word = NULL;
      gboolean correctly_spelled;
      GError *error = NULL;

      if (!gbp_spell_utils_text_iter_starts_word (&word_start))
        {
          GtkTextIter iter;

          iter = word_start;
          gbp_spell_utils_text_iter_forward_word_end (&word_start);

          if (gtk_text_iter_equal (&iter, &word_start))
            return FALSE;

          gbp_spell_utils_text_iter_backward_word_start (&word_start);
        }

        if (!gbp_spell_utils_skip_no_spell_check (no_spell_check_tag, &word_start, &end))
          return FALSE;

        g_return_val_if_fail (gbp_spell_utils_text_iter_starts_word (&word_start), FALSE);

        word_end = word_start;
        gbp_spell_utils_text_iter_forward_word_end (&word_end);

        if (gtk_text_iter_compare (&end, &word_end) < 0)
          return FALSE;

        word = gtk_text_buffer_get_text (self->buffer, &word_start, &word_end, FALSE);
        correctly_spelled = gspell_checker_check_word (spell_checker, word, -1, &error);

        if (error != NULL)
          {
            g_propagate_error (error_p, error);
            return FALSE;
          }

        if (!correctly_spelled)
          {
            /* Found! */
            gtk_text_buffer_move_mark (self->buffer, self->word_start, &word_start);
            gtk_text_buffer_move_mark (self->buffer, self->word_end, &word_end);
            gbp_spell_navigator_select_misspelled_word (self);

            if (spell_checker_p != NULL)
              *spell_checker_p = g_object_ref (spell_checker);

            if (word_p != NULL)
              *word_p = g_steal_pointer (&word);

            return TRUE;
         }

        word_start = word_end;
    }

  return FALSE;
}

static void
gbp_spell_navigator_change (GspellNavigator *navigator,
                            const gchar     *word,
                            const gchar     *change_to)
{
  GbpSpellNavigator *self = (GbpSpellNavigator *)navigator;
  GtkTextIter word_start;
  GtkTextIter word_end;
  g_autofree gchar *word_in_buffer = NULL;

  g_assert (GBP_IS_SPELL_NAVIGATOR (self));
  g_assert (GTK_IS_TEXT_MARK (self->word_start));
  g_assert (GTK_IS_TEXT_MARK (self->word_end));

  gtk_text_buffer_get_iter_at_mark (self->buffer, &word_start, self->word_start);
  gtk_text_buffer_get_iter_at_mark (self->buffer, &word_end, self->word_end);

  word_in_buffer = gtk_text_buffer_get_slice (self->buffer, &word_start, &word_end, TRUE);
  g_return_if_fail (word_in_buffer != NULL);
  g_return_if_fail (g_strcmp0 (word_in_buffer, word) == 0);

  gtk_text_buffer_begin_user_action (self->buffer);

  gtk_text_buffer_delete (self->buffer, &word_start, &word_end);
  gtk_text_buffer_insert (self->buffer, &word_start, change_to, -1);

  gtk_text_buffer_end_user_action (self->buffer);
}

static void
gbp_spell_navigator_change_all (GspellNavigator *navigator,
                                const gchar     *word,
                                const gchar     *change_to)
{
  GbpSpellNavigator *self = (GbpSpellNavigator *)navigator;
  GtkTextIter iter;

  g_assert (GBP_IS_SPELL_NAVIGATOR (self));
  g_assert (GTK_IS_TEXT_MARK (self->start_boundary));
  g_assert (GTK_IS_TEXT_MARK (self->end_boundary));

  gtk_text_buffer_get_iter_at_mark (self->buffer, &iter, self->start_boundary);
  gtk_text_buffer_begin_user_action (self->buffer);

  while (TRUE)
    {
      gboolean found;
      GtkTextIter match_start;
      GtkTextIter match_end;
      GtkTextIter limit;

      gtk_text_buffer_get_iter_at_mark (self->buffer, &limit, self->end_boundary);
      found = gtk_text_iter_forward_search (&iter,
                                            word,
                                            GTK_TEXT_SEARCH_VISIBLE_ONLY |
                                            GTK_TEXT_SEARCH_TEXT_ONLY,
                                            &match_start,
                                            &match_end,
                                            &limit);

      if (!found)
        break;

      if (gbp_spell_utils_text_iter_starts_word (&match_start) &&
          gbp_spell_utils_text_iter_ends_word (&match_end))
        {
          gtk_text_buffer_delete (self->buffer, &match_start, &match_end);
          gtk_text_buffer_insert (self->buffer, &match_end, change_to, -1);
        }

      iter = match_end;
    }

  gtk_text_buffer_end_user_action (self->buffer);
}

static void
gspell_navigator_iface_init (GspellNavigatorInterface *iface)
{
  iface->goto_next = gbp_spell_navigator_goto_next;
  iface->change = gbp_spell_navigator_change;
  iface->change_all = gbp_spell_navigator_change_all;
}
