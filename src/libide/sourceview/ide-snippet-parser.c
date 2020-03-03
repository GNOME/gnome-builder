/* ide-snippet-parser.c
 *
 * Copyright 2013-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-snippet-parser"

#include "config.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <libide-io.h>
#include <stdlib.h>

#include "ide-snippet.h"
#include "ide-snippet-chunk.h"
#include "ide-snippet-parser.h"
#include "ide-snippet-private.h"

/**
 * SECTION:ide-snippet-parser
 * @title: IdeSnippetParser
 * @short_description: A parser for Builder's snippet text format
 *
 * The #IdeSnippetParser can be used to parse ".snippets" formatted
 * text files. This is generally only used internally by Builder, but can
 * be used by plugins under certain situations.
 *
 * Since: 3.32
 */

struct _IdeSnippetParser
{
  GObject  parent_instance;

  GList   *snippets;

  gint     lineno;
  GList   *chunks;
  GList   *scope;
  gchar   *cur_name;
  gchar   *cur_desc;
  GString *cur_text;
  GString *snippet_text;

  GFile   *current_file;

  guint    had_error : 1;
};

G_DEFINE_TYPE (IdeSnippetParser, ide_snippet_parser, G_TYPE_OBJECT)

enum {
  PARSING_ERROR,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

IdeSnippetParser *
ide_snippet_parser_new (void)
{
  return g_object_new (IDE_TYPE_SNIPPET_PARSER, NULL);
}

static void
ide_snippet_parser_flush_chunk (IdeSnippetParser *parser)
{
  IdeSnippetChunk *chunk;

  if (parser->cur_text->len)
    {
      chunk = ide_snippet_chunk_new ();
      ide_snippet_chunk_set_spec (chunk, parser->cur_text->str);
      parser->chunks = g_list_append (parser->chunks, chunk);
      g_string_truncate (parser->cur_text, 0);
    }
}

static void
ide_snippet_parser_store (IdeSnippetParser *parser)
{
  IdeSnippet *snippet;
  GList *scope_iter;
  GList *chunck_iter;

  ide_snippet_parser_flush_chunk (parser);
  for (scope_iter = parser->scope; scope_iter; scope_iter = scope_iter->next)
    {
      snippet = ide_snippet_new (parser->cur_name, scope_iter->data);
      ide_snippet_set_description (snippet, parser->cur_desc);

      for (chunck_iter = parser->chunks; chunck_iter; chunck_iter = chunck_iter->next)
        {
#if 0
          g_printerr ("%s:  Tab: %02d  Link: %02d  Text: %s\n",
                      parser->cur_name,
                      ide_snippet_chunk_get_tab_stop (chunck_iter->data),
                      ide_snippet_chunk_get_linked_chunk (chunck_iter->data),
                      ide_snippet_chunk_get_text (chunck_iter->data));
#endif
          ide_snippet_add_chunk (snippet, chunck_iter->data);
        }

      parser->snippets = g_list_append (parser->snippets, snippet);
    }
}

static void
ide_snippet_parser_finish (IdeSnippetParser *parser)
{
  if (parser->cur_name)
    ide_snippet_parser_store(parser);

  g_clear_pointer (&parser->cur_name, g_free);

  g_string_truncate (parser->cur_text, 0);
  g_string_truncate (parser->snippet_text, 0);

  g_list_foreach (parser->chunks, (GFunc) g_object_unref, NULL);
  g_list_free (parser->chunks);
  parser->chunks = NULL;

  g_list_free_full (parser->scope, g_free);
  parser->scope = NULL;

  g_free (parser->cur_desc);
  parser->cur_desc = NULL;
}

static void
ide_snippet_parser_do_part_simple (IdeSnippetParser *parser,
                                   const gchar      *line)
{
  g_string_append (parser->cur_text, line);
}

static void
ide_snippet_parser_do_part_n (IdeSnippetParser *parser,
                              gint              n,
                              const gchar      *inner)
{
  IdeSnippetChunk *chunk;

  g_return_if_fail (IDE_IS_SNIPPET_PARSER (parser));
  g_return_if_fail (n >= -1);
  g_return_if_fail (inner);

  chunk = ide_snippet_chunk_new ();
  ide_snippet_chunk_set_spec (chunk, n ? inner : "");
  ide_snippet_chunk_set_tab_stop (chunk, n);
  parser->chunks = g_list_append (parser->chunks, chunk);
}

static void
ide_snippet_parser_do_part_linked (IdeSnippetParser *parser,
                                   gint              n)
{
  IdeSnippetChunk *chunk;
  gchar text[12];

  chunk = ide_snippet_chunk_new ();
  if (n)
    {
      g_snprintf (text, sizeof text, "$%d", n);
      text[sizeof text - 1] = '\0';
      ide_snippet_chunk_set_spec (chunk, text);
    }
  else
    {
      ide_snippet_chunk_set_spec (chunk, "");
      ide_snippet_chunk_set_tab_stop (chunk, 0);
    }
  parser->chunks = g_list_append (parser->chunks, chunk);
}

static void
ide_snippet_parser_do_part_named (IdeSnippetParser *parser,
                                  const gchar      *name)
{
  IdeSnippetChunk *chunk;
  gchar *spec;

  chunk = ide_snippet_chunk_new ();
  spec = g_strdup_printf ("$%s", name);
  ide_snippet_chunk_set_spec (chunk, spec);
  ide_snippet_chunk_set_tab_stop (chunk, -1);
  parser->chunks = g_list_append (parser->chunks, chunk);
  g_free (spec);
}

static gboolean
parse_variable (const gchar  *line,
                glong        *n,
                gchar       **inner,
                const gchar **endptr,
                gchar       **name)
{
  gboolean has_inner = FALSE;
  char *end = NULL;
  gint brackets;
  gint i;

  *n = -1;
  *inner = NULL;
  *endptr = NULL;
  *name = NULL;

  g_assert (*line == '$');

  line++;

  *endptr = line;

  if (!*line)
    {
      *endptr = NULL;
      return FALSE;
    }

  if (*line == '{')
    {
      has_inner = TRUE;
      line++;
    }

  if (g_ascii_isdigit (*line))
    {
      errno = 0;
      *n = strtol (line, &end, 10);
      if (((*n == LONG_MIN) || (*n == LONG_MAX)) && errno == ERANGE)
        return FALSE;
      else if (*n < 0)
        return FALSE;
      line = end;
    }
  else if (g_ascii_isalpha (*line))
    {
      const gchar *cur;

      for (cur = line; *cur; cur++)
        {
          if (g_ascii_isalnum (*cur))
            continue;
          break;
        }
      *endptr = cur;
      *name = g_strndup (line, cur - line);
      *n = -2;
      return TRUE;
    }

  if (has_inner)
    {
      if (*line == ':')
        line++;

      brackets = 1;

      for (i = 0; line[i]; i++)
        {
          switch (line[i])
            {
            case '{':
              brackets++;
              break;

            case '}':
              brackets--;
              break;

            default:
              break;
            }

          if (!brackets)
            {
              *inner = g_strndup (line, i);
              *endptr = &line[i + 1];
              return TRUE;
            }
        }

      return FALSE;
    }

  *endptr = line;

  return TRUE;
}

static void
ide_snippet_parser_do_part (IdeSnippetParser *parser,
                            const gchar      *line)
{
  const gchar *dollar;
  gchar *str;
  gchar *inner;
  gchar *name;
  glong n;

  g_assert (line);
  g_assert (*line == '\t');

  line++;

again:
  if (!*line)
    return;

  if (!(dollar = strchr (line, '$')))
    {
      ide_snippet_parser_do_part_simple (parser, line);
      return;
    }

  /*
   * Parse up to the next $ as a simple.
   * If it is $N or ${N} then it is a linked chunk w/o tabstop.
   * If it is ${N:""} then it is a chunk w/ tabstop.
   * If it is ${blah|upper} then it is a non-tab stop chunk performing
   * some sort of of expansion.
   */

  g_assert (dollar >= line);

  if (dollar != line)
    {
      str = g_strndup (line, (dollar - line));
      ide_snippet_parser_do_part_simple (parser, str);
      g_free (str);
      line = dollar;
    }

parse_dollar:
  inner = NULL;

  if (!parse_variable (line, &n, &inner, &line, &name))
    {
      ide_snippet_parser_do_part_simple (parser, line);
      return;
    }

#if 0
  g_printerr ("Parse Variable: N=%d  inner=\"%s\"\n", n, inner);
  g_printerr ("  Left over: \"%s\"\n", line);
#endif

  ide_snippet_parser_flush_chunk (parser);

  if (inner)
    {
      ide_snippet_parser_do_part_n (parser, n, inner);
      g_free (inner);
      inner = NULL;
    }
  else if (n == -2 && name)
    ide_snippet_parser_do_part_named (parser, name);
  else
    ide_snippet_parser_do_part_linked (parser, n);

  g_free (name);

  if (line)
    {
      if (*line == '$')
        {
          goto parse_dollar;
        }
      else
        goto again;
    }
}

static void
ide_snippet_parser_do_snippet (IdeSnippetParser *parser,
                               const gchar      *line)
{
  parser->cur_name = g_strstrip (g_strdup (&line[8]));
}

static void
ide_snippet_parser_do_snippet_scope (IdeSnippetParser *parser,
                                     const gchar      *line)
{
  gchar **scope_list;
  GList *iter;
  gint i;
  gboolean add_scope;

  scope_list = g_strsplit (&line[8], ",", -1);

  for (i = 0; scope_list[i]; i++)
    {
      add_scope = TRUE;
      for (iter = parser->scope; iter; iter = iter->next)
        {
          if (g_strcmp0 (iter->data, scope_list[i]) == 0)
            {
              add_scope = FALSE;
              break;
            }
        }

      if (add_scope)
        parser->scope = g_list_append(parser->scope, g_strstrip (g_strdup (scope_list[i])));
    }

  g_strfreev(scope_list);
}

static void
ide_snippet_parser_do_snippet_description (IdeSnippetParser *parser,
                                           const gchar      *line)
{
  if (parser->cur_desc)
    {
      g_free(parser->cur_desc);
      parser->cur_desc = NULL;
    }

  parser->cur_desc = g_strstrip (g_strdup (&line[7]));
}

static void
ide_snippet_parser_feed_line (IdeSnippetParser *parser,
                              const gchar      *basename,
                              const gchar      *line)
{
  const gchar *orig = line;

  g_assert (parser);
  g_assert (basename);
  g_assert (line);

  parser->lineno++;

  switch (*line)
    {
    case '\0':
      if (parser->cur_name)
        g_string_append_c (parser->cur_text, '\n');
      break;

    case '#':
      break;

    case '\t':
      if (parser->cur_name)
        {
          GList *iter;
          gboolean add_default_scope = TRUE;
          for (iter = parser->scope; iter; iter = iter->next)
            {
              if (g_strcmp0(iter->data, basename) == 0)
                {
                  add_default_scope = FALSE;
                  break;
                }
            }

          if (add_default_scope)
            parser->scope = g_list_append(parser->scope,
                                        g_strstrip (g_strdup (basename)));

          if (parser->cur_text->len || parser->chunks)
            g_string_append_c (parser->cur_text, '\n');
          ide_snippet_parser_do_part (parser, line);
        }
      break;

    case 's':
      if (g_str_has_prefix (line, "snippet"))
        {
          ide_snippet_parser_finish (parser);
          ide_snippet_parser_do_snippet (parser, line);
          break;
        }

    /* Fall through */
    case '-':
      if (parser->cur_text->len || parser->chunks)
        {
          ide_snippet_parser_store(parser);

          g_string_truncate (parser->cur_text, 0);

          g_list_foreach (parser->chunks, (GFunc) g_object_unref, NULL);
          g_list_free (parser->chunks);
          parser->chunks = NULL;

          g_list_free_full(parser->scope, g_free);
          parser->scope = NULL;
        }

      if (g_str_has_prefix(line, "- scope"))
        {
          ide_snippet_parser_do_snippet_scope (parser, line);
          break;
        }

      if (g_str_has_prefix(line, "- desc"))
        {
          ide_snippet_parser_do_snippet_description (parser, line);
          break;
        }

    /* Fall through */
    default:
      g_signal_emit (parser, signals [PARSING_ERROR], 0,
                     parser->current_file, parser->lineno, line);
      parser->had_error = TRUE;
      break;
    }

  g_string_append (parser->snippet_text, orig);
  g_string_append_c (parser->snippet_text, '\n');
}

gboolean
ide_snippet_parser_load_from_file (IdeSnippetParser  *parser,
                                   GFile             *file,
                                   GError           **error)
{
  GFileInputStream *file_stream;
  g_autoptr(GDataInputStream) data_stream = NULL;
  GError *local_error = NULL;
  gchar *line;
  gchar *basename = NULL;

  g_return_val_if_fail (IDE_IS_SNIPPET_PARSER (parser), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  basename = g_file_get_basename (file);

  if (basename)
    {
      if (strstr (basename, "."))
        *strstr (basename, ".") = '\0';
    }

  file_stream = g_file_read (file, NULL, error);
  if (!file_stream)
    return FALSE;

  data_stream = g_data_input_stream_new (G_INPUT_STREAM (file_stream));
  g_object_unref (file_stream);

  g_set_object (&parser->current_file, file);

again:
  if (parser->had_error)
    {
      /* TODO: Better error messages */
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "%s:%d: invalid snippet",
                   basename, parser->lineno);
      return FALSE;
    }

  line = g_data_input_stream_read_line_utf8 (data_stream, NULL, NULL, &local_error);
  if (!line && local_error)
    {
      g_propagate_error (error, local_error);
      g_set_object (&parser->current_file, NULL);
      return FALSE;
    }
  else if (line)
    {
      ide_snippet_parser_feed_line (parser, basename, line);
      g_free (line);
      goto again;
    }

  ide_snippet_parser_finish (parser);
  g_free (basename);

  g_set_object (&parser->current_file, NULL);

  return TRUE;
}

gboolean
ide_snippet_parser_load_from_data (IdeSnippetParser  *parser,
                                   const gchar       *default_language,
                                   const gchar       *data,
                                   gssize             data_len,
                                   GError           **error)
{
  IdeLineReader reader;
  gchar *line;
  gsize line_len;

  g_return_val_if_fail (IDE_IS_SNIPPET_PARSER (parser), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (data_len < 0)
    data_len = strlen (data);

  ide_line_reader_init (&reader, (gchar *)data, data_len);

  while ((line = ide_line_reader_next (&reader, &line_len)))
    {
      g_autofree gchar *copy = NULL;

      if (parser->had_error)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "<data>:%d: invalid snippet",
                       parser->lineno);
          return FALSE;
        }

      copy = g_strndup (line, line_len);
      ide_snippet_parser_feed_line (parser, default_language, copy);
    }

  ide_snippet_parser_finish (parser);

  return TRUE;
}

/**
 * ide_snippet_parser_get_snippets:
 * @parser: a #IdeSnippetParser
 *
 * Get the list of all the snippets loaded.
 *
 * Returns: (transfer none) (element-type Ide.Snippet): a #GList of #IdeSnippets items.
 *
 * Since: 3.32
 */
GList *
ide_snippet_parser_get_snippets (IdeSnippetParser *parser)
{
  g_return_val_if_fail (IDE_IS_SNIPPET_PARSER (parser), NULL);
  return parser->snippets;
}

static void
ide_snippet_parser_finalize (GObject *object)
{
  IdeSnippetParser *self = IDE_SNIPPET_PARSER (object);

  g_list_foreach (self->snippets, (GFunc) g_object_unref, NULL);
  g_list_free (self->snippets);
  self->snippets = NULL;

  g_list_foreach (self->chunks, (GFunc) g_object_unref, NULL);
  g_list_free (self->chunks);
  self->chunks = NULL;

  g_list_free_full(self->scope, g_free);
  self->scope = NULL;

  if (self->cur_text)
    g_string_free (self->cur_text, TRUE);
  self->cur_text = NULL;

  if (self->snippet_text)
    g_string_free (self->snippet_text, TRUE);
  self->snippet_text = NULL;

  g_free (self->cur_name);
  self->cur_name = NULL;

  if (self->cur_desc)
    {
      g_free (self->cur_desc);
      self->cur_desc = NULL;
    }

  G_OBJECT_CLASS (ide_snippet_parser_parent_class)->finalize (object);
}

static void
ide_snippet_parser_class_init (IdeSnippetParserClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = ide_snippet_parser_finalize;

  signals [PARSING_ERROR] =
    g_signal_new ("parsing-error",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_FILE,
                  G_TYPE_UINT,
                  G_TYPE_STRING);
}

static void
ide_snippet_parser_init (IdeSnippetParser *parser)
{
  parser->lineno = -1;
  parser->cur_text = g_string_new (NULL);
  parser->snippet_text = g_string_new (NULL);
  parser->scope = NULL;
  parser->cur_desc = NULL;
}

/**
 * ide_snippet_parser_parse_one:
 * @data: the data to parse
 * @data_len: the length of data, or -1 for %NULL terminated
 * @error: a location for an error
 *
 * Returns: (transfer full): an #IdeSnippet
 *
 * Since: 3.36
 */
IdeSnippet *
ide_snippet_parser_parse_one (const char  *data,
                              gssize       data_len,
                              GError     **error)
{
  g_autoptr(IdeSnippetParser) parser = NULL;
  IdeLineReader reader;
  const gchar *line;
  gsize line_len;

  g_return_val_if_fail (data != NULL, NULL);

  parser = ide_snippet_parser_new ();
  ide_snippet_parser_feed_line (parser, "", "snippet dummy");

  ide_line_reader_init (&reader, (gchar *)data, data_len);
  while ((line = ide_line_reader_next (&reader, &line_len)))
    {
      g_autofree gchar *copy = NULL;

      if (parser->had_error)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "<data>:%d: invalid snippet",
                       parser->lineno);
          return NULL;
        }

      copy = g_malloc (line_len + 2);
      copy[0] = '\t';
      memcpy (&copy[1], line, line_len);
      copy[1+line_len] = 0;

      ide_snippet_parser_feed_line (parser, "", copy);
    }

  ide_snippet_parser_finish (parser);

  if (parser->snippets != NULL)
    return g_object_ref (parser->snippets->data);

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_DATA,
               "<data>:%d: invalid snippet",
               parser->lineno);

  return NULL;
}
