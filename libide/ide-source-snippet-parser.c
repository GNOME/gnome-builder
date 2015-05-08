/* ide-source-snippet-parser.c
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#include <errno.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "ide-source-snippet.h"
#include "ide-source-snippet-chunk.h"
#include "ide-source-snippet-parser.h"
#include "ide-source-snippet-private.h"

G_DEFINE_TYPE (IdeSourceSnippetParser, ide_source_snippet_parser, G_TYPE_OBJECT)

struct _IdeSourceSnippetParserPrivate
{
  GList   *snippets;

  gint     lineno;
  GList   *chunks;
  GList   *scope;
  gchar   *cur_name;
  gchar   *cur_desc;
  GString *cur_text;
};

IdeSourceSnippetParser *
ide_source_snippet_parser_new (void)
{
  return g_object_new (IDE_TYPE_SOURCE_SNIPPET_PARSER, NULL);
}

static void
ide_source_snippet_parser_flush_chunk (IdeSourceSnippetParser *parser)
{
  IdeSourceSnippetParserPrivate *priv = parser->priv;
  IdeSourceSnippetChunk *chunk;

  if (priv->cur_text->len)
    {
      chunk = ide_source_snippet_chunk_new ();
      ide_source_snippet_chunk_set_spec (chunk, priv->cur_text->str);
      priv->chunks = g_list_append (priv->chunks, chunk);
      g_string_truncate (priv->cur_text, 0);
    }
}

static void
ide_source_snippet_parser_store (IdeSourceSnippetParser *parser)
{
  IdeSourceSnippetParserPrivate *priv = parser->priv;
  IdeSourceSnippet *snippet;
  GList *scope_iter;
  GList *chunck_iter;

  ide_source_snippet_parser_flush_chunk (parser);
  for (scope_iter = priv->scope; scope_iter; scope_iter = scope_iter->next)
    {
      snippet = ide_source_snippet_new (priv->cur_name, g_strdup(scope_iter->data));
      ide_source_snippet_set_description(snippet, priv->cur_desc);

      for (chunck_iter = priv->chunks; chunck_iter; chunck_iter = chunck_iter->next)
        {
        #if 0
          g_printerr ("%s:  Tab: %02d  Link: %02d  Text: %s\n",
                      parser->priv->cur_name,
                      ide_source_snippet_chunk_get_tab_stop (chunck_iter->data),
                      ide_source_snippet_chunk_get_linked_chunk (chunck_iter->data),
                      ide_source_snippet_chunk_get_text (chunck_iter->data));
        #endif
          ide_source_snippet_add_chunk (snippet, chunck_iter->data);
        }

      priv->snippets = g_list_append (priv->snippets, snippet);
    }
}

static void
ide_source_snippet_parser_finish (IdeSourceSnippetParser *parser)
{
  IdeSourceSnippetParserPrivate *priv = parser->priv;

  if (priv->cur_name)
    {
      ide_source_snippet_parser_store(parser);
    }

  g_clear_pointer (&priv->cur_name, g_free);

  g_string_truncate (priv->cur_text, 0);

  g_list_foreach (priv->chunks, (GFunc) g_object_unref, NULL);
  g_list_free (priv->chunks);
  priv->chunks = NULL;

  g_list_free_full(priv->scope, g_free);
  priv->scope = NULL;

  g_free(priv->cur_desc);
  priv->cur_desc = NULL;
}

static void
ide_source_snippet_parser_do_part_simple (IdeSourceSnippetParser *parser,
                                         const gchar           *line)
{
  g_string_append (parser->priv->cur_text, line);
}

static void
ide_source_snippet_parser_do_part_n (IdeSourceSnippetParser *parser,
                                    gint                   n,
                                    const gchar           *inner)
{
  IdeSourceSnippetParserPrivate *priv = parser->priv;
  IdeSourceSnippetChunk *chunk;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPET_PARSER (parser));
  g_return_if_fail (n >= -1);
  g_return_if_fail (inner);

  chunk = ide_source_snippet_chunk_new ();
  ide_source_snippet_chunk_set_spec (chunk, n ? inner : "");
  ide_source_snippet_chunk_set_tab_stop (chunk, n);
  priv->chunks = g_list_append (priv->chunks, chunk);
}

static void
ide_source_snippet_parser_do_part_linked (IdeSourceSnippetParser *parser,
                                         gint                   n)
{
  IdeSourceSnippetParserPrivate *priv = parser->priv;
  IdeSourceSnippetChunk *chunk;
  gchar text[12];

  chunk = ide_source_snippet_chunk_new ();
  if (n)
    {
      g_snprintf (text, sizeof text, "$%d", n);
      text[sizeof text - 1] = '\0';
      ide_source_snippet_chunk_set_spec (chunk, text);
    }
  else
    {
      ide_source_snippet_chunk_set_spec (chunk, "");
      ide_source_snippet_chunk_set_tab_stop (chunk, 0);
    }
  priv->chunks = g_list_append (priv->chunks, chunk);
}

static void
ide_source_snippet_parser_do_part_named (IdeSourceSnippetParser *parser,
                                        const gchar           *name)
{
  IdeSourceSnippetParserPrivate *priv = parser->priv;
  IdeSourceSnippetChunk *chunk;
  gchar *spec;

  chunk = ide_source_snippet_chunk_new ();
  spec = g_strdup_printf ("$%s", name);
  ide_source_snippet_chunk_set_spec (chunk, spec);
  ide_source_snippet_chunk_set_tab_stop (chunk, -1);
  priv->chunks = g_list_append (priv->chunks, chunk);
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
ide_source_snippet_parser_do_part (IdeSourceSnippetParser *parser,
                                  const gchar           *line)
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
      ide_source_snippet_parser_do_part_simple (parser, line);
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
      ide_source_snippet_parser_do_part_simple (parser, str);
      g_free (str);
      line = dollar;
    }

parse_dollar:
  inner = NULL;

  if (!parse_variable (line, &n, &inner, &line, &name))
    {
      ide_source_snippet_parser_do_part_simple (parser, line);
      return;
    }

#if 0
  g_printerr ("Parse Variable: N=%d  inner=\"%s\"\n", n, inner);
  g_printerr ("  Left over: \"%s\"\n", line);
#endif

  ide_source_snippet_parser_flush_chunk (parser);

  if (inner)
    {
      ide_source_snippet_parser_do_part_n (parser, n, inner);
      g_free (inner);
      inner = NULL;
    }
  else if (n == -2 && name)
    ide_source_snippet_parser_do_part_named (parser, name);
  else
    ide_source_snippet_parser_do_part_linked (parser, n);

  g_free (name);

  if (line)
    {
      if (*line == '$')
        {
          dollar = line;
          goto parse_dollar;
        }
      else
        goto again;
    }
}

static void
ide_source_snippet_parser_do_snippet (IdeSourceSnippetParser *parser,
                                     const gchar           *line)
{
  parser->priv->cur_name = g_strstrip (g_strdup (&line[8]));
}

static void
ide_source_snippet_parser_do_snippet_scope (IdeSourceSnippetParser *parser,
                                           const gchar           *line)
{
  IdeSourceSnippetParserPrivate *priv = parser->priv;
  gchar **scope_list;
  GList *iter;
  gint i;
  gboolean add_scope;

  scope_list = g_strsplit (&line[8], ",", -1);

  for (i = 0; scope_list[i]; i++)
    {
      add_scope = TRUE;
      for (iter = priv->scope; iter; iter = iter->next)
        {
          if (g_strcmp0(iter->data, scope_list[i]) == 0)
              add_scope = FALSE;
              break;
        }

      if (add_scope)
        priv->scope = g_list_append(priv->scope,
                                    g_strstrip (g_strdup (scope_list[i])));
    }

  g_strfreev(scope_list);
}

static void
ide_source_snippet_parser_do_snippet_description (IdeSourceSnippetParser *parser,
                                                 const gchar           *line)
{
  IdeSourceSnippetParserPrivate *priv = parser->priv;

  if (priv->cur_desc)
    {
      g_free(priv->cur_desc);
      priv->cur_desc = NULL;
    }

  priv->cur_desc = g_strstrip (g_strdup (&line[7]));
}

static void
ide_source_snippet_parser_feed_line (IdeSourceSnippetParser *parser,
                                    gchar                 *basename,
                                    const gchar           *line)
{
  IdeSourceSnippetParserPrivate *priv = parser->priv;

  g_assert (parser);
  g_assert (basename);
  g_assert (line);

  priv->lineno++;

  switch (*line)
    {
    case '\0':
      if (priv->cur_name)
        g_string_append_c (priv->cur_text, '\n');
      break;

    case '#':
      break;

    case '\t':
      if (priv->cur_name)
        {
          GList *iter;
          gboolean add_default_scope = TRUE;
          for (iter = priv->scope; iter; iter = iter->next)
            {
              if (g_strcmp0(iter->data, basename) == 0)
                {
                  add_default_scope = FALSE;
                  break;
                }
            }

          if (add_default_scope)
            priv->scope = g_list_append(priv->scope,
                                        g_strstrip (g_strdup (basename)));

          if (priv->cur_text->len || priv->chunks)
            g_string_append_c (priv->cur_text, '\n');
          ide_source_snippet_parser_do_part (parser, line);
        }
      break;

    case 's':
      if (g_str_has_prefix (line, "snippet"))
        {
          ide_source_snippet_parser_finish (parser);
          ide_source_snippet_parser_do_snippet (parser, line);
          break;
        }

    case '-':
      if (priv->cur_text->len || priv->chunks)
        {
          ide_source_snippet_parser_store(parser);

          g_string_truncate (priv->cur_text, 0);

          g_list_foreach (priv->chunks, (GFunc) g_object_unref, NULL);
          g_list_free (priv->chunks);
          priv->chunks = NULL;

          g_list_free_full(priv->scope, g_free);
          priv->scope = NULL;
        }

      if (g_str_has_prefix(line, "- scope"))
        {
          ide_source_snippet_parser_do_snippet_scope (parser, line);
          break;
        }

      if (g_str_has_prefix(line, "- desc"))
        {
          ide_source_snippet_parser_do_snippet_description (parser, line);
          break;
        }

    /* Fall through */
    default:
      g_warning (_("Invalid snippet at line %d: %s"), priv->lineno, line);
      break;
    }
}

gboolean
ide_source_snippet_parser_load_from_file (IdeSourceSnippetParser *parser,
                                         GFile                 *file,
                                         GError               **error)
{
  GFileInputStream *file_stream;
  GDataInputStream *data_stream;
  GError *local_error = NULL;
  gchar *line;
  gchar *basename = NULL;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET_PARSER (parser), FALSE);
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

again:
  line = g_data_input_stream_read_line_utf8 (data_stream, NULL, NULL, &local_error);
  if (line && local_error)
    {
      g_propagate_error (error, local_error);
      return FALSE;
    }
  else if (line)
    {
      ide_source_snippet_parser_feed_line (parser, basename, line);
      g_free (line);
      goto again;
    }

  ide_source_snippet_parser_finish (parser);
  g_free(basename);

  return TRUE;
}

GList *
ide_source_snippet_parser_get_snippets (IdeSourceSnippetParser *parser)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET_PARSER (parser), NULL);
  return parser->priv->snippets;
}

static void
ide_source_snippet_parser_finalize (GObject *object)
{
  IdeSourceSnippetParserPrivate *priv;

  priv = IDE_SOURCE_SNIPPET_PARSER (object)->priv;

  g_list_foreach (priv->snippets, (GFunc) g_object_unref, NULL);
  g_list_free (priv->snippets);
  priv->snippets = NULL;

  g_list_foreach (priv->chunks, (GFunc) g_object_unref, NULL);
  g_list_free (priv->chunks);
  priv->chunks = NULL;

  g_list_free_full(priv->scope, g_free);
  priv->scope = NULL;

  if (priv->cur_text)
    g_string_free (priv->cur_text, TRUE);

  g_free (priv->cur_name);
  priv->cur_name = NULL;

  if (priv->cur_desc)
    {
      g_free (priv->cur_desc);
      priv->cur_desc = NULL;
    }

  G_OBJECT_CLASS (ide_source_snippet_parser_parent_class)->finalize (object);
}

static void
ide_source_snippet_parser_class_init (IdeSourceSnippetParserClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = ide_source_snippet_parser_finalize;
  g_type_class_add_private (object_class,
                            sizeof (IdeSourceSnippetParserPrivate));
}

static void
ide_source_snippet_parser_init (IdeSourceSnippetParser *parser)
{
  parser->priv = G_TYPE_INSTANCE_GET_PRIVATE (parser,
                                              IDE_TYPE_SOURCE_SNIPPET_PARSER,
                                              IdeSourceSnippetParserPrivate);
  parser->priv->lineno = -1;
  parser->priv->cur_text = g_string_new (NULL);
  parser->priv->scope = NULL;
  parser->priv->cur_desc = NULL;
}
