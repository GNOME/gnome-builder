#include <libide-sourceview.h>
#include <stdlib.h>

gint
main (gint   argc,
      gchar *argv[])
{
  GOptionContext *context;
  GError *error = NULL;
  gint i;

  context = g_option_context_new ("[FILES...] - test snippet parsing");

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  for (i = 1; i < argc; i++)
    {
      const gchar *filename = argv [i];
      g_autoptr(IdeSnippetParser) parser = NULL;
      g_autoptr(GFile) file = NULL;

      if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
        {
          g_printerr ("Failed to open %s\n", filename);
          return EXIT_FAILURE;
        }

      file = g_file_new_for_commandline_arg (filename);

      parser = ide_snippet_parser_new ();

      if (!ide_snippet_parser_load_from_file (parser, file, &error))
        {
          g_printerr ("%s\n", error->message);
          return EXIT_FAILURE;
        }

      {
        for (GList *iter = ide_snippet_parser_get_snippets (parser);
             iter != NULL;
             iter = iter->next)
          {
            IdeSnippet *snippet = iter->data;

            g_print ("=====================================\n");
            g_print ("Snippet: %s with language %s\n",
                     ide_snippet_get_trigger (snippet),
                     ide_snippet_get_language (snippet));

            for (guint j = 0; j < ide_snippet_get_n_chunks (snippet); j++)
              {
                IdeSnippetChunk *chunk = ide_snippet_get_nth_chunk (snippet, j);
                gint tab_stop = ide_snippet_chunk_get_tab_stop (chunk);

                if (tab_stop > 0)
                  g_print ("TAB STOP %02d (%02d): %s\n", tab_stop, j, ide_snippet_chunk_get_spec (chunk));
                else
                  g_print ("TEXT        (%02d): %s\n", j, ide_snippet_chunk_get_spec (chunk));
              }
          }
      }
    }

  g_option_context_free (context);

  return EXIT_SUCCESS;
}
