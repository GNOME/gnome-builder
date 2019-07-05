/* devhelp2-parser.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include "devhelp2-parser.h"

static const gchar *
devhelp2_parser_intern (Devhelp2Parser *state,
                        const gchar    *str)
{
  if (str == NULL)
    return NULL;

  return g_string_chunk_insert_const (state->strings, str);
}

static Chapter *
chapter_new (const gchar *name,
             const gchar *link)
{
  Chapter *chapter;

  chapter = g_slice_new0 (Chapter);
  chapter->list.data = chapter;
  chapter->name = name;
  chapter->link = link;

  return chapter;
}

static void
chapter_free (Chapter *chapter)
{
  if (chapter->parent)
    g_queue_unlink (&chapter->parent->children, &chapter->list);

  chapter->parent = NULL;

  while (chapter->children.head)
    chapter_free (chapter->children.head->data);

  g_assert (chapter->parent == NULL);
  g_assert (chapter->list.prev == NULL);
  g_assert (chapter->list.next == NULL);
  g_assert (chapter->children.length == 0);

  g_slice_free (Chapter, chapter);
}

static void
chapter_append (Chapter *chapter,
                Chapter *child)
{
  g_assert (chapter != NULL);
  g_assert (child != NULL);
  g_assert (child->parent == NULL);

  child->parent = chapter;
  g_queue_push_tail_link (&chapter->children, &child->list);
}

static void
devhelp2_parser_start_element (GMarkupParseContext  *context G_GNUC_UNUSED,
                               const gchar          *element_name,
                               const gchar         **attribute_names,
                               const gchar         **attribute_values,
                               gpointer              user_data,
                               GError              **error)
{
  Devhelp2Parser *state = user_data;

  g_assert (element_name != NULL);
  g_assert (state != NULL);
  g_assert (state->context != NULL);

  if (g_str_equal (element_name, "book"))
    {
      const gchar *title = NULL;
      const gchar *link = NULL;
      const gchar *author = NULL;
      const gchar *name = NULL;
      const gchar *version = NULL;
      const gchar *language = NULL;
      const gchar *online = NULL;
      const gchar *xmlns = NULL;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "title", &title,
                                        G_MARKUP_COLLECT_STRING, "link", &link,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_OPTIONAL | G_MARKUP_COLLECT_STRING, "author", &author,
                                        G_MARKUP_COLLECT_OPTIONAL | G_MARKUP_COLLECT_STRING, "version", &version,
                                        G_MARKUP_COLLECT_OPTIONAL | G_MARKUP_COLLECT_STRING, "language", &language,
                                        G_MARKUP_COLLECT_OPTIONAL | G_MARKUP_COLLECT_STRING, "online", &online,
                                        G_MARKUP_COLLECT_OPTIONAL | G_MARKUP_COLLECT_STRING, "xmlns", &xmlns,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      state->book.title = devhelp2_parser_intern (state, title);
      state->book.link = devhelp2_parser_intern (state, link);
      state->book.author = devhelp2_parser_intern (state, author);
      state->book.name = devhelp2_parser_intern (state, name);
      state->book.version = devhelp2_parser_intern (state, version);
      state->book.language = devhelp2_parser_intern (state, language);
      state->book.online = devhelp2_parser_intern (state, online);
    }
  else if (g_str_equal (element_name, "sub"))
    {
      const gchar *name = NULL;
      const gchar *link = NULL;
      Chapter *chapter;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "link", &link,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      chapter = chapter_new (devhelp2_parser_intern (state, name),
                             devhelp2_parser_intern (state, link));

      if (state->chapter != NULL)
        chapter_append (state->chapter, chapter);

      state->chapter = chapter;
    }
  else if (g_str_equal (element_name, "keyword"))
    {
      Keyword keyword;
      const gchar *name = NULL;
      const gchar *link = NULL;
      const gchar *type = NULL;
      const gchar *since = NULL;
      const gchar *deprecated = NULL;
      const gchar *stability = NULL;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING, "type", &type,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "link", &link,
                                        G_MARKUP_COLLECT_OPTIONAL | G_MARKUP_COLLECT_STRING, "deprecated", &deprecated,
                                        G_MARKUP_COLLECT_OPTIONAL | G_MARKUP_COLLECT_STRING, "since", &since,
                                        G_MARKUP_COLLECT_OPTIONAL | G_MARKUP_COLLECT_STRING, "stability", &stability,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      keyword.type = devhelp2_parser_intern (state, type);
      keyword.kind = GPOINTER_TO_UINT (g_hash_table_lookup (state->kinds, keyword.type));
      keyword.name = devhelp2_parser_intern (state, name);
      keyword.link = devhelp2_parser_intern (state, link);
      keyword.since = devhelp2_parser_intern (state, since);
      keyword.deprecated = devhelp2_parser_intern (state, deprecated);
      keyword.stability = devhelp2_parser_intern (state, stability);

      g_array_append_val (state->keywords, keyword);
    }
}

static void
devhelp2_parser_end_element (GMarkupParseContext  *context G_GNUC_UNUSED,
                             const gchar          *element_name,
                             gpointer              user_data,
                             GError              **error G_GNUC_UNUSED)
{
  Devhelp2Parser *state = user_data;

  if (g_str_equal (element_name, "sub"))
    {
      if (state->chapter->parent)
        state->chapter = state->chapter->parent;
    }
}

static GMarkupParser devhelp2_parser = {
  .start_element = devhelp2_parser_start_element,
  .end_element = devhelp2_parser_end_element,
};

Devhelp2Parser *
devhelp2_parser_new (void)
{
  Devhelp2Parser *state;

  state = g_slice_new0 (Devhelp2Parser);
  state->kinds = g_hash_table_new (NULL, NULL);
  state->strings = g_string_chunk_new (4096*4L);
  state->keywords = g_array_new (FALSE, FALSE, sizeof (Keyword));
  state->context = g_markup_parse_context_new (&devhelp2_parser,
                                               G_MARKUP_IGNORE_QUALIFIED,
                                               state, NULL);

#define ADD_KIND(k, v) \
  g_hash_table_insert (state->kinds, \
                       (gchar *)g_string_chunk_insert_const (state->strings, k), \
                       GUINT_TO_POINTER(v))

  ADD_KIND ("function", IDE_DOCS_ITEM_KIND_FUNCTION);
  ADD_KIND ("struct", IDE_DOCS_ITEM_KIND_STRUCT);
  ADD_KIND ("enum", IDE_DOCS_ITEM_KIND_ENUM);
  ADD_KIND ("property", IDE_DOCS_ITEM_KIND_PROPERTY);
  ADD_KIND ("signal", IDE_DOCS_ITEM_KIND_SIGNAL);
  ADD_KIND ("macro", IDE_DOCS_ITEM_KIND_MACRO);
  ADD_KIND ("member", IDE_DOCS_ITEM_KIND_MEMBER);
  ADD_KIND ("method", IDE_DOCS_ITEM_KIND_METHOD);
  ADD_KIND ("constant", IDE_DOCS_ITEM_KIND_CONSTANT);

#undef ADD_KIND

  return state;
}

void
devhelp2_parser_free (Devhelp2Parser *state)
{
  g_clear_pointer (&state->kinds, g_hash_table_unref);
  g_clear_pointer (&state->context, g_markup_parse_context_free);
  g_clear_pointer (&state->strings, g_string_chunk_free);
  g_clear_pointer (&state->keywords, g_array_unref);
  g_clear_pointer (&state->chapter, chapter_free);
  g_clear_pointer (&state->directory, g_free);
  g_slice_free (Devhelp2Parser, state);
}

gboolean
devhelp2_parser_parse_file (Devhelp2Parser  *state,
                            const gchar     *filename,
                            GError         **error)
{
  g_autofree gchar *contents = NULL;
  gboolean ret = FALSE;
  gint fd;

  g_assert (state != NULL);
  g_assert (filename != NULL);
  g_assert (state->directory == NULL);

  state->directory = g_path_get_dirname (filename);

  fd = g_open (filename, O_RDONLY, 0);

  if (fd == -1)
    {
      int errsv = errno;
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errsv),
                   "%s",
                   g_strerror (errno));
      goto failure;
    }

  for (;;)
    {
      gchar buf[4096*4L];
      gssize n_read;

      n_read = read (fd, buf, sizeof buf);

      if (n_read > 0)
        {
          if (!g_markup_parse_context_parse (state->context, buf, n_read, error))
            goto failure;
        }
      else if (n_read < 0)
        {
          int errsv = errno;
          g_set_error (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errsv),
                       "%s",
                       g_strerror (errno));
          goto failure;
        }
      else
        {
          g_assert (n_read == 0);
          break;
        }
    }

  ret = g_markup_parse_context_end_parse (state->context, error);

failure:
  close (fd);

  return ret;
}
