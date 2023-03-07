/* test-spell-cursor.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include <locale.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "editor-spell-cursor.c"
#include "cjhtextregion.c"

#undef WANT_DISPLAY_TESTS

static const char *test_text = "this is a series of words  ";
static const char *test_text_2 = "it's possible we're going to have join-words.";
#ifdef WANT_DISPLAY_TESTS
static const char *test_text_3 = "\
/* ide-buffer.c\
 *\
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>\
 *\
 * This program is free software: you can redistribute it and/or modify\
 * it under the terms of the GNU General Public License as published by\
 * the Free Software Foundation, either version 3 of the License, or\
 * (at your option) any later version.\
 *\
 * This program is distributed in the hope that it will be useful,\
 * but WITHOUT ANY WARRANTY; without even the implied warranty of\
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\
 * GNU General Public License for more details.\
 *\
 * You should have received a copy of the GNU General Public License\
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.\
 *\
 * SPDX-License-Identifier: GPL-3.0-or-later\
 */\
";
#endif

static char *
next_word (EditorSpellCursor *cursor)
{
  GtkTextIter begin, end;

  if (editor_spell_cursor_next (cursor, &begin, &end))
    return gtk_text_iter_get_slice (&begin, &end);

  return NULL;
}

static void
test_cursor (void)
{
  g_autoptr(GtkTextBuffer) buffer = gtk_text_buffer_new (NULL);
  CjhTextRegion *region = _cjh_text_region_new (NULL, NULL);
  g_autoptr(EditorSpellCursor) cursor = editor_spell_cursor_new (buffer, region, NULL, NULL);
  char *word;

  gtk_text_buffer_set_text (buffer, test_text, -1);
  _cjh_text_region_insert (region, 0, strlen (test_text), NULL);

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "this");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "is");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "a");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "series");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "of");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "words");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, NULL);

  _cjh_text_region_free (region);
}

#ifdef WANT_DISPLAY_TESTS
static void
test_cursor2 (void)
{
  g_autoptr(GtkTextBuffer) buffer = GTK_TEXT_BUFFER (gtk_source_buffer_new (NULL));
  GtkWindow *window;
  GtkSourceView *view;
  CjhTextRegion *region = _cjh_text_region_new (NULL, NULL);
  g_autoptr(EditorSpellCursor) cursor = editor_spell_cursor_new (buffer, region, NULL, NULL);
  GtkSourceLanguage *l = gtk_source_language_manager_get_language (gtk_source_language_manager_get_default (), "c");
  static const char *words[] = {
    "ide", "buffer", "c",
    "Copyright", "2018", "2019", "Christian", "Hergert", "chergert", "redhat", "com",
    "This", "program", "is", "free", "software", "you", "can", "redistribute", "it", "and", "or", "modify",
    "it", "under", "the", "terms", "of", "the", "GNU", "General", "Public", "License", "as", "published", "by",
    "the", "Free", "Software", "Foundation", "either", "version", "3", "of", "the", "License", "or",
    "at", "your", "option", "any", "later", "version",
  };

  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), l);
  gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (buffer), TRUE);
  gtk_text_buffer_set_text (buffer, test_text_3, -1);
  _cjh_text_region_insert (region, 0, strlen (test_text_3), NULL);

  window = g_object_new (GTK_TYPE_WINDOW, NULL);
  view = GTK_SOURCE_VIEW (gtk_source_view_new ());
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (view), buffer);
  gtk_window_set_child (window, GTK_WIDGET (view));
  gtk_window_present (window);

  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, TRUE);

  for (guint i = 0; i < G_N_ELEMENTS (words); i++)
    {
      char *word = next_word (cursor);
      g_assert_cmpstr (word, ==, words[i]);
      g_free (word);
    }

  _cjh_text_region_free (region);
}
#endif

static void
test_cursor_in_word (void)
{
  g_autoptr(GtkTextBuffer) buffer = gtk_text_buffer_new (NULL);
  CjhTextRegion *region = _cjh_text_region_new (NULL, NULL);
  g_autoptr(EditorSpellCursor) cursor = editor_spell_cursor_new (buffer, region, NULL, NULL);
  const char *pos = strstr (test_text, "ries "); /* se|ries */
  gsize offset = pos - test_text;
  char *word;

  gtk_text_buffer_set_text (buffer, test_text, -1);
  _cjh_text_region_insert (region, 0, strlen (test_text), GINT_TO_POINTER (1));
  _cjh_text_region_replace (region, offset, strlen (test_text) - offset, NULL);

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "series");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "of");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "words");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, NULL);

  _cjh_text_region_free (region);
}

static void
test_cursor_join_words (void)
{
  g_autoptr(GtkTextBuffer) buffer = gtk_text_buffer_new (NULL);
  CjhTextRegion *region = _cjh_text_region_new (NULL, NULL);
  g_autoptr(EditorSpellCursor) cursor = editor_spell_cursor_new (buffer, region, NULL, "-'");
  char *word;

  gtk_text_buffer_set_text (buffer, test_text_2, -1);
  _cjh_text_region_insert (region, 0, strlen (test_text_2), NULL);

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "it's");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "possible");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "we're");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "going");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "to");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "have");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, "join-words");

  word = next_word (cursor);
  g_assert_cmpstr (word, ==, NULL);

  _cjh_text_region_free (region);
}

int
main (int argc,
      char *argv[])
{
  setlocale (LC_ALL, "C");
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

#ifdef WANT_DISPLAY_TESTS
  gtk_init ();
  gtk_source_init ();
#endif

  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Spelling/Cursor/basic", test_cursor);
#ifdef WANT_DISPLAY_TESTS
  g_test_add_func ("/Spelling/Cursor/basic2", test_cursor2);
#endif
  g_test_add_func ("/Spelling/Cursor/in_word", test_cursor_in_word);
  g_test_add_func ("/Spelling/Cursor/join_words", test_cursor_join_words);
  return g_test_run ();
}
