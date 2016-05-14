/* tmpl-token-input-stream.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tmpl-token-input-stream.h"

struct _TmplTokenInputStream
{
  GDataInputStream parent_instance;
  guint            swallow_newline : 1;
  guint            last_was_text_with_newline : 1;
};

G_DEFINE_TYPE (TmplTokenInputStream, tmpl_token_input_stream, G_TYPE_DATA_INPUT_STREAM)

static void
tmpl_token_input_stream_class_init (TmplTokenInputStreamClass *klass)
{
}

static void
tmpl_token_input_stream_init (TmplTokenInputStream *self)
{
  self->last_was_text_with_newline = TRUE;
}

static gboolean
tmpl_token_input_stream_read_unichar (TmplTokenInputStream  *self,
                                      gunichar              *unichar,
                                      GCancellable          *cancellable,
                                      GError               **error)
{
  GBufferedInputStream *stream = (GBufferedInputStream *)self;
  gchar str[8] = { 0 };
  gint c;
  gint n;
  gint i;

  g_assert (TMPL_IS_TOKEN_INPUT_STREAM (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (-1 == (c = g_buffered_input_stream_read_byte (stream, cancellable, error)))
    return FALSE;

  if ((c & 0x80) == 0)
    n = 1;
  else if ((c & 0xE0) == 0xC0)
    n = 2;
  else if ((c & 0xF0) == 0xE0)
    n = 3;
  else if ((c & 0xF8) == 0xF0)
    n = 4;
  else if ((c & 0xFC) == 0xF8)
    n = 5;
  else if ((c & 0xFE) == 0xFC)
    n = 6;
  else
    n = 0;

  str [0] = c;

  for (i = 1; i < n; i++)
    {
      if (-1 == (c = g_buffered_input_stream_read_byte (stream, cancellable, error)))
        return FALSE;

      str [i] = (gchar)c;
    }

  *unichar = g_utf8_get_char (str);

  return TRUE;
}

static gchar *
tmpl_token_input_stream_read_tag (TmplTokenInputStream  *self,
                                  gsize                 *length,
                                  GCancellable          *cancellable,
                                  GError               **error)
{
  GBufferedInputStream *stream = (GBufferedInputStream *)self;
  GByteArray *ar;
  GError *local_error = NULL;
  gboolean in_string = FALSE;
  guchar byte;
  gint c;

  g_assert (TMPL_IS_TOKEN_INPUT_STREAM (self));
  g_assert (length != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ar = g_byte_array_new ();

  while (TRUE)
    {
      if (-1 == (c = g_buffered_input_stream_read_byte (stream, cancellable, &local_error)))
        goto failure;

      switch (c)
        {
        case '\\':
          if (in_string)
            {
              g_byte_array_append (ar, (const guchar *)"\\", 1);

              if (-1 == (c = g_buffered_input_stream_read_byte (stream, cancellable, &local_error)))
                goto failure;
            }

          break;

        case '"':
          in_string = !in_string;
          break;

        case '}':
          if (!in_string)
            {
              if (-1 == (c = g_buffered_input_stream_read_byte (stream, cancellable, &local_error)))
                goto failure;

              /* Check if we got }} */
              if (c == '}')
                goto finish;

              g_byte_array_append (ar, (const guchar *)"}", 1);
            }

          break;

        default:
          break;
        }

      byte = (guchar)c;
      g_byte_array_append (ar, (const guchar *)&byte, 1);
    }

finish:
  *length = ar->len;

  byte = 0;
  g_byte_array_append (ar, (const guchar *)&byte, 1);

  return (gchar *)g_byte_array_free (ar, FALSE);

failure:
  *length = 0;

  g_byte_array_free (ar, TRUE);

  if (local_error)
    g_propagate_error (error, local_error);

  return FALSE;
}

/**
 * tmpl_token_input_stream_read_token:
 * @self: An #TmplTokenInputStream
 * @error: (nullable): An optional location for a #GError
 *
 * Reads the next token from the underlying stream.
 *
 * If there was an error, %NULL is returned and @error is set.
 *
 * Returns: (transfer full): A #TmplToken or %NULL.
 */
TmplToken *
tmpl_token_input_stream_read_token (TmplTokenInputStream  *self,
                                    GCancellable          *cancellable,
                                    GError               **error)
{
  GDataInputStream *stream = (GDataInputStream *)self;
  GError *local_error = NULL;
  gunichar ch;
  gchar *text;
  gsize len;

  g_return_val_if_fail (TMPL_IS_TOKEN_INPUT_STREAM (self), NULL);

  /*
   * The syntax of the template language is very simple. All of our symbols
   * start with {{ and end with }}. We use \ to escape, and you only ever
   * need to escape the opening of {{ like \{{.
   *
   * We scan ahead until a { or \ and take appropriate action based upon
   * peeking at the next char.
   *
   * Once we resolve that, we walk forward past the expression until }}.
   * To walk past the expression, we need to know when we are in a
   * string, since }} could theoretically be in there too.
   */

  text = g_data_input_stream_read_upto (stream, "\\{", -1, &len, cancellable, error);

  /*
   * Handle end of stream.
   */
  if (text == NULL)
    return NULL;

  /*
   * If we start with a newline, and need to swallow it (as can happen if the
   * last tag was at the end of the line), skip past the newline.
   */
  if (self->swallow_newline && *text == '\n')
    {
      gchar *tmp = g_strdup (text + 1);
      g_free (text);
      text = tmp;
    }

  self->swallow_newline = FALSE;

  /*
   * Handle successful read up to \ or {.
   */
  if (*text != '\0')
    {
      self->last_was_text_with_newline = g_str_has_suffix (text, "\n");

      return tmpl_token_new_text (text);
    }

  g_free (text);

  /*
   * Peek what type of delimiter we hit.
   */
  ch = g_data_input_stream_read_byte (stream, cancellable, &local_error);

  if ((ch == 0) && (local_error != NULL))
    {
      g_propagate_error (error, local_error);
      return NULL;
    }

  /*
   * Handle possible escaped \{.
   */
  if (ch == '\\')
    {
      gchar str[8] = { 0 };

      self->last_was_text_with_newline = FALSE;

      /*
       * Get the next char after \.
       */
      if (!tmpl_token_input_stream_read_unichar (self, &ch, cancellable, error))
        return tmpl_token_new_unichar ('\\');

      /*
       * Handle escaping {.
       */
      if (ch == '{')
        return tmpl_token_new_unichar ('{');

      /*
       * Nothing escaped, return string as it was read.
       */
      g_unichar_to_utf8 (ch, str);

      return tmpl_token_new_text (g_strdup_printf ("\\%s", str));
    }

  g_assert (ch == '{');

  /*
   * Look for { following {. If we reached the end of the stream, just
   * return a token for the final {.
   */
  if (!tmpl_token_input_stream_read_unichar (self, &ch, cancellable, error))
    {
      self->last_was_text_with_newline = FALSE;
      return tmpl_token_new_unichar ('{');
    }

  /*
   * If this is not a {{, then just return a string for the pair.
   */
  if (ch != '{')
    {
      gchar str[8] = { 0 };

      g_unichar_to_utf8 (ch, str);

      self->last_was_text_with_newline = FALSE;

      return tmpl_token_new_text (g_strdup_printf ("{%s", str));
    }

  /*
   * Scan ahead until we find }}.
   */
  if (!(text = tmpl_token_input_stream_read_tag (self, &len, cancellable, error)))
    return NULL;

  self->swallow_newline = self->last_was_text_with_newline;
  self->last_was_text_with_newline = FALSE;

  return tmpl_token_new_generic (text);
}

/**
 * tmpl_token_input_stream_new:
 * @base_stream: the stream to read from
 *
 * Creates a #TmplTokenInputStream using @base_stream for the raw
 * text stream.
 *
 * Returns: (transfer full): An #TmplTokenInputStream.
 */
TmplTokenInputStream *
tmpl_token_input_stream_new (GInputStream *base_stream)
{
  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  return g_object_new (TMPL_TYPE_TOKEN_INPUT_STREAM,
                       "base-stream", base_stream,
                       NULL);
}
