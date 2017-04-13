#include "egg-suggestion-entry-buffer.h"

static gchar *
suggest_suffix (EggSuggestion *suggestion,
                const gchar   *query,
                const gchar   *suffix)
{
  return g_strdup (suffix);
}

static void
test_basic (void)
{
  g_autoptr(EggSuggestionEntryBuffer) buffer = NULL;
  g_autoptr(EggSuggestion) suggestion = NULL;
  g_autoptr(EggSuggestion) suggestion2 = NULL;
  const gchar *text;
  guint len;
  guint n_chars;

  buffer = egg_suggestion_entry_buffer_new ();

  suggestion = egg_suggestion_new ();
  egg_suggestion_set_id (suggestion, "some-id");
  egg_suggestion_set_title (suggestion, "this is the title");
  egg_suggestion_set_subtitle (suggestion, "this is the subtitle");
  egg_suggestion_set_icon_name (suggestion, "gtk-missing-symbolic");
  g_signal_connect (suggestion, "suggest-suffix", G_CALLBACK (suggest_suffix), "abcd");

  suggestion2 = egg_suggestion_new ();
  g_signal_connect (suggestion2, "suggest-suffix", G_CALLBACK (suggest_suffix), "99999");

  egg_suggestion_entry_buffer_set_suggestion (buffer, suggestion);
  g_assert (suggestion == egg_suggestion_entry_buffer_get_suggestion (buffer));

  gtk_entry_buffer_insert_text (GTK_ENTRY_BUFFER (buffer), 0, "1234", 4);

  len = gtk_entry_buffer_get_length (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpint (len, ==, 8);

  text = gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpstr (text, ==, "1234abcd");

  n_chars = gtk_entry_buffer_insert_text (GTK_ENTRY_BUFFER (buffer), 4, "z", 1);
  g_assert_cmpint (n_chars, ==, 1);

  len = gtk_entry_buffer_get_length (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpint (len, ==, 9);

  text = gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpstr (text, ==, "1234zabcd");

  n_chars = gtk_entry_buffer_delete_text (GTK_ENTRY_BUFFER (buffer), 1, 1);
  g_assert_cmpint (n_chars, ==, 1);

  len = gtk_entry_buffer_get_length (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpint (len, ==, 8);

  text = gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpstr (text, ==, "134zabcd");

  egg_suggestion_entry_buffer_set_suggestion (buffer, NULL);

  len = gtk_entry_buffer_get_length (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpint (len, ==, 4);

  text = gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpstr (text, ==, "134z");

  egg_suggestion_entry_buffer_set_suggestion (buffer, suggestion2);

  len = gtk_entry_buffer_get_length (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpint (len, ==, 9);

  text = gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpstr (text, ==, "134z99999");

  egg_suggestion_entry_buffer_set_suggestion (buffer, suggestion);

  len = gtk_entry_buffer_get_length (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpint (len, ==, 8);

  text = gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpstr (text, ==, "134zabcd");

  /* Fail by trying to delete the extended text */
  len = gtk_entry_buffer_delete_text (GTK_ENTRY_BUFFER (buffer), 4, 4);
  g_assert_cmpint (len, ==, 0);

  text = gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (buffer));
  g_assert_cmpstr (text, ==, "134zabcd");
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gtk_init (&argc, &argv);
  g_test_add_func ("/Egg/SuggestionEntryBuffer/basic", test_basic);
  return g_test_run ();
}
