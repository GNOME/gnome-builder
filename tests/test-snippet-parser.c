#include <ide.h>
#include <stdlib.h>

#include "ide-source-snippet-parser.h"

gint
main (gint   argc,
      gchar *argv[])
{
  GOptionContext *context;
  GOptionEntry entries[] = {
    NULL
  };
  GError *error = NULL;
  gint i;

  context = g_option_context_new ("[FILES...] - test snippet parsing");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  for (i = 1; i < argc; i++)
    {
      const gchar *filename = argv [i];
      g_autoptr(IdeSourceSnippetParser) parser = NULL;
      g_autoptr(GFile) file = NULL;

      if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
        {
          g_printerr ("Failed to open %s\n", filename);
          return EXIT_FAILURE;
        }

      file = g_file_new_for_commandline_arg (filename);

      parser = ide_source_snippet_parser_new ();

      if (!ide_source_snippet_parser_load_from_file (parser, file, &error))
        {
          g_printerr ("%s\n", error->message);
          return EXIT_FAILURE;
        }
    }

  g_option_context_free (context);

  return EXIT_SUCCESS;
}
