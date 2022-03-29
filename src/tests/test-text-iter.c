/* test-text-iter.c
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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

#include <libide-sourceview.h>

static void
test_current_symbol (void)
{
  g_autoptr(GtkTextBuffer) buffer = GTK_TEXT_BUFFER (gtk_source_buffer_new (NULL));
  GtkSourceLanguageManager *m = gtk_source_language_manager_get_default ();
  GtkSourceLanguage *l = gtk_source_language_manager_get_language (m, "c");
  static const gchar *expected[] = {
    NULL, NULL, NULL, NULL, NULL,
    "c", "co", "con", "cons", "const",
    NULL,
    "g", "gc", "gch", "gcha", "gchar",
    NULL, NULL,
    "s", "st", "str",
    NULL, NULL, NULL,
    "g", "g_", "g_s", "g_st", "g_str", "g_strd", "g_strdu", "g_strdup",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  };

  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), l);
  gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (buffer), TRUE);
  gtk_text_buffer_set_text (buffer, "  { const gchar *str = g_strdup (\"something\"); }", -1);

  /* Update syntax data immediately for gsv context classes */
  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, FALSE);

  for (guint i = 0; i < G_N_ELEMENTS (expected); i++)
    {
      g_autofree gchar *word = NULL;
      GtkTextIter iter;

      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, i);
      word = ide_text_iter_current_symbol (&iter, NULL);

      g_assert_cmpstr (word, ==, expected[i]);
    }
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gtk_init ();
  g_test_add_func ("/Ide/TextIter/current_symbol", test_current_symbol);
  return g_test_run ();
}
