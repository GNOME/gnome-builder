#include <glib.h>

const gchar **parse_languages (gchar *language_string);

static void
meson_test_parse_languages (void)
{
  struct {
    gchar *input;
    gchar **expected;
  } cases[] = {
      { .input = "'testproject', 'rust'", .expected = (gchar*[]){ "rust", NULL } },
      { .input = "'testproject', 'rust', 'c'", .expected = (gchar*[]){ "rust", "c", NULL } },
      { .input = "'testproject', 'rust', version: '3.0'", .expected = (gchar*[]){ "rust", NULL } },
      { .input = "testproject, rust, version: 3.0, default_options: ['warning_level=2']", .expected = (gchar*[]){ "rust", NULL } },
      { .input = "testproject\n\n,\n rust, \nversion: 3.0, default_options: ['warning_level=2']", .expected = (gchar*[]){ "rust", NULL } },
      { .input = "testproject\n\n,\n ['rust'], \nversion: 3.0, default_options: ['warning_level=2']", .expected = (gchar*[]){ "rust", NULL } },
      { .input = "testproject\n\n,\n ['rust']", .expected = (gchar*[]){ "rust", NULL } },
      { .input = "testproject\n\n,\n ['c', 'c++'], \nversion: 3.0, default_options: ['warning_level=2']", .expected = (gchar*[]){ "c", "c++", NULL } },
      { .input = "testproject\n\n,\n ['c', 'c++', \nversion: 3.0", .expected = NULL},
      { .input = "testproject\n\n,\n 'c', 'c++', \nversion: 3.0", .expected = (gchar*[]){ "c", "c++", NULL } },
      { .input = "testproject\n\n,\n 'c', 'c++'], \nversion: 3.0", .expected = NULL },
      { .input = "'testproject',\nversion: 3.0", .expected = NULL },
      { .input = "'projectname'", .expected = NULL },
  };

  const guint n_cases = G_N_ELEMENTS (cases);

  for (guint i = 0; i < n_cases; i++)
    {
      const gchar **languages = parse_languages (cases[i].input);

      g_assert_cmpstrv (cases[i].expected, languages);
    }
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func("/meson/parse_languages", meson_test_parse_languages);

  return g_test_run ();
}
