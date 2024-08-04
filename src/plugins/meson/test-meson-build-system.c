#include <glib.h>

char **_gbp_meson_build_system_parse_languages (const char *language_string);

static void
meson_test_parse_languages (void)
{
  const struct {
    const char *input;
    const char **expected;
  } cases[] = {
      { .input = "'testproject', 'rust',", .expected = (const char *[]){ "rust", NULL } },
      { .input = "'testproject', 'rust', 'c'", .expected = (const char *[]){ "rust", "c", NULL } },
      { .input = "'testproject', 'rust', version: '3.0'", .expected = (const char *[]){ "rust", NULL } },
      { .input = "testproject, rust, version: 3.0, default_options: ['warning_level=2']", .expected = (const char *[]){ "rust", NULL } },
      { .input = "testproject\n\n,\n rust, \nversion: 3.0, default_options: ['warning_level=2']", .expected = (const char *[]){ "rust", NULL } },
      { .input = "testproject\n\n,\n ['rust'], \nversion: 3.0, default_options: ['warning_level=2']", .expected = (const char *[]){ "rust", NULL } },
      { .input = "testproject\n\n,\n ['rust']", .expected = (const char *[]){ "rust", NULL } },
      { .input = "testproject\n\n,\n ['c', 'c++'], \nversion: 3.0, default_options: ['warning_level=2']", .expected = (const char *[]){ "c", "c++", NULL } },
      { .input = "testproject\n\n,\n ['c', 'c++', \nversion: 3.0", .expected = (const char *[]){ "c", "c++", NULL } },
      { .input = "testproject\n\n,\n 'c', 'c++', \nversion: 3.0", .expected = (const char *[]){ "c", "c++", NULL } },
      { .input = "testproject\n\n,\n 'c', 'c++'], \nversion: 3.0", .expected = (const char *[]){ "c", "c++", NULL } },
      { .input = "'testproject',\nversion: 3.0", .expected = NULL },
      { .input = "'projectname'", .expected = NULL },
  };

  const guint n_cases = G_N_ELEMENTS (cases);

  for (guint i = 0; i < n_cases; i++)
    {
      g_auto(GStrv) languages = _gbp_meson_build_system_parse_languages (cases[i].input);

      if (languages)
        {
          g_print ("%s\n", cases[i].input);
          for (guint j = 0; languages[j]; j++)
            g_print ("> %s\n", languages[j]);
        }

      g_assert_cmpstrv (cases[i].expected, languages);
    }
}

gint
main (gint  argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func("/meson/parse_languages", meson_test_parse_languages);
  return g_test_run ();
}
