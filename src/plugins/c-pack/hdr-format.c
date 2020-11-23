/* hdr-format.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "hdr-format"

#include <gtksourceview/gtksource.h>
#include <string.h>

#include "c-parse-helper.h"
#include "hdr-format.h"

typedef struct
{
  gchar     *pre;
  gchar     *return_type;
  gchar     *identifier;
  GSList    *params;
  gchar     *post;
} Chunk;

static void
clear_chunk (Chunk *chunk)
{
  g_clear_pointer (&chunk->pre, g_free);
  g_clear_pointer (&chunk->return_type, g_free);
  g_clear_pointer (&chunk->identifier, g_free);
  g_clear_pointer (&chunk->post, g_free);
  g_slist_free_full (chunk->params, (GDestroyNotify)parameter_free);
  chunk->params = NULL;
}

static void
skip_space (const gchar **str)
{
  const gchar *p = *str;
  while (*p && g_ascii_isspace (*p))
    p++;
  *str = p;
}

static gboolean
read_char (const gchar  *str,
           gchar         ch,
           const gchar **pos)
{
  g_assert (str);
  g_assert (ch);
  g_assert (pos);

  skip_space (&str);
  if (*str != ch)
    return FALSE;
  *pos = str + 1;
  return TRUE;
}

static gchar *
getword (const gchar  *str,
         const gchar **endptr)
{
  skip_space (&str);

  if (*str == '*')
    {
      *endptr = str + 1;
      return g_strdup ("*");
    }

  for (const gchar *pos = str; *pos; pos = g_utf8_next_char (pos))
    {
      gunichar ch = g_utf8_get_char (pos);

      if (ch == '*' || ch == '(' || g_unichar_isspace (ch))
        {
          *endptr = pos;
          return g_strndup (str, pos - str);
        }
    }

  return NULL;
}

static gchar *
read_attr (const gchar  *str,
           const gchar **pos)
{
  g_autofree gchar *word = NULL;
  const gchar *tmp;

  skip_space (&str);

  if (g_str_has_prefix (str, "__attribute__"))
    {
      const gchar *begin = str;
      gint count = 0;

      for (; *str; str++)
        {
          if (*str == '(')
            count++;
          else if (*str == ')')
            {
              if (count == 0)
                {
                  str++;
                  *pos = str;
                  return g_strndup (begin, str - begin);
                }
              count--;
            }
        }

      goto failure;
    }

  if (!(word = getword (str, &tmp)))
    goto failure;

  if (g_str_has_prefix (word, "G_GNUC_") ||
      strstr (word, "AVAILABLE") ||
      strstr (word, "INTERNAL") ||
      strcasestr (word, "export"))
    {
      *pos = tmp;
      return g_steal_pointer (&word);
    }

failure:
  *pos = str;
  return NULL;
}

static gchar *
read_return_type (const gchar  *str,
                  const gchar **pos)
{
  g_autoptr(GString) gstr = g_string_new (NULL);
  gboolean word_found = FALSE;

  for (;;)
    {
      g_autofree gchar *word = NULL;
      const gchar *tmp;

      if (!(word = getword (str, &tmp)))
        return NULL;

      if (g_str_equal (word, "static") ||
          g_str_equal (word, "const") ||
          g_str_equal (word, "struct") ||
          g_str_equal (word, "enum") ||
          *word == '*')
        {
          if (gstr->len)
            g_string_append_c (gstr, ' ');
          g_string_append (gstr, word);
          str = tmp;
        }
      else if (!word_found)
        {
          if (gstr->len)
            g_string_append_c (gstr, ' ');
          g_string_append (gstr, word);
          word_found = TRUE;
          str = tmp;
        }
      else
        break;
    }

  *pos = str;

  return g_string_free (g_steal_pointer (&gstr), FALSE);
}

static gboolean
push_chunk (GArray      *ar,
            const gchar *str)
{
  g_autofree gchar *attr = NULL;
  g_autofree gchar *return_type = NULL;
  g_autofree gchar *ident = NULL;
  const gchar *pos;
  Chunk chunk = {0};

  g_assert (ar);
  g_assert (str);

  if ((attr = read_attr (str, &pos)))
    {
      chunk.pre = g_strstrip (g_steal_pointer (&attr));
      str = pos;
    }

  if (!(return_type = read_return_type (str, &pos)))
    goto failure;
  str = pos;

  chunk.return_type = g_strstrip (g_steal_pointer (&return_type));

  if (!(ident = getword (str, &pos)))
    goto failure;
  if (*ident != '_' && !g_ascii_isalpha (*ident))
    goto failure;
  str = pos;
  chunk.identifier = g_strstrip (g_steal_pointer (&ident));

  if (!read_char (str, '(', &pos))
    goto failure;
  str = pos;

  /* get params */
  if (!(pos = strchr (str, ')')))
    goto failure;

  {
    g_autofree gchar *inner = g_strndup (str, pos - str);
    chunk.params = parse_parameters (inner);
    str = pos;
  }

  if (!read_char (str, ')', &pos))
    goto failure;
  str = pos;

  chunk.post = g_strstrip (g_strdup (str));
  if (chunk.post[0] == 0)
    g_clear_pointer (&chunk.post, g_free);

  g_array_append_val (ar, chunk);

  return TRUE;

failure:

  clear_chunk (&chunk);

  return FALSE;
}

static void
parse_rtype (const gchar  *type,
             gchar       **rtype,
             guint        *n_star)
{
  g_autofree gchar *rev = g_utf8_strreverse (type, -1);
  const gchar *tmp = rev;

  *n_star = 0;

  for (;;)
    {
      if (!*tmp)
        break;

      if (*tmp == '*' || g_ascii_isspace (*tmp))
        *n_star += *tmp == '*';
      else
        break;

      tmp++;
    }

  *rtype = g_utf8_strreverse (tmp, -1);
}

gchar *
hdr_format_string (const gchar *data,
                   gssize       len)
{
  g_autofree gchar *copy = NULL;
  g_autoptr(GString) out = NULL;
  g_autoptr(GArray) ar = NULL;
  g_auto(GStrv) chunks = NULL;
  guint long_ret = 0;
  guint long_ret_star = 0;
  guint long_ident = 0;
  guint long_ptype = 0;
  guint long_star = 0;

  ar = g_array_new (FALSE, FALSE, sizeof (Chunk));
  g_array_set_clear_func (ar, (GDestroyNotify)clear_chunk);

  if (data == NULL || *data == 0)
    return g_strdup ("");

  if (len < 0)
    len = strlen (data);

  copy = g_strndup (data, len);

  chunks = g_strsplit (copy, ";", 0);

  for (guint i = 0; chunks[i]; i++)
    {
      g_strstrip (chunks[i]);

      if (!*chunks[i])
        continue;

      if (!push_chunk (ar, chunks[i]))
        return NULL;
    }

  if (ar->len == 0)
    return NULL;

  out = g_string_new (NULL);

#if 0
  for (guint i = 0; i < ar->len; i++)
    {
      const Chunk *chunk = &g_array_index (ar, Chunk, i);
    }
#endif

  for (guint i = 0; i < ar->len; i++)
    {
      const Chunk *chunk = &g_array_index (ar, Chunk, i);
      g_autofree gchar *rtype = NULL;
      guint n_star = 0;

      parse_rtype (chunk->return_type, &rtype, &n_star);

      long_ret = MAX (long_ret, strlen (rtype));
      long_ret_star = MAX (long_ret_star, n_star);

      long_ident = MAX (long_ident, strlen (chunk->identifier));

      for (const GSList *iter = chunk->params; iter; iter = iter->next)
        {
          Parameter *p = iter->data;

          long_star = MAX (long_star, p->n_star);
          if (p->type)
            long_ptype = MAX (long_ptype, strlen (p->type));
        }
    }

  for (guint i = 0; i < ar->len; i++)
    {
      const Chunk *chunk = &g_array_index (ar, Chunk, i);
      guint n_star = 0;
      guint off;
      guint space;
      guint rlen;

      if (chunk->pre)
        g_string_append_printf (out, "%s\n", chunk->pre);

      off = out->len;

      if (chunk->return_type)
        {
          g_autofree gchar *rtype = NULL;

          parse_rtype (chunk->return_type, &rtype, &n_star);
          rlen = strlen (rtype);

          g_string_append (out, rtype);
        }
      else
        {
          g_string_append (out, "void");
          rlen = 4;
        }

      for (guint j = rlen; j < long_ret; j++)
        g_string_append_c (out, ' ');
      g_string_append_c (out, ' ');

      for (guint j = 0; j < long_ret_star - n_star; j++)
        g_string_append_c (out, ' ');
      for (guint j = 0; j < n_star; j++)
        g_string_append_c (out, '*');

      g_string_append (out, chunk->identifier);
      rlen = strlen (chunk->identifier);
      for (guint j = rlen; j < long_ident; j++)
        g_string_append_c (out, ' ');

      g_string_append (out, " (");
      space = out->len - off;

      if (chunk->params == NULL)
        g_string_append (out, "void");

      for (const GSList *iter = chunk->params; iter; iter = iter->next)
        {
          Parameter *p = iter->data;

          if (p->ellipsis)
            {
              g_string_append (out, "...");
              break;
            }

          if (p->type == NULL)
            {
              g_warning ("Unexpected NULL value for type");
              continue;
            }

          g_string_append (out, p->type);

          for (guint j = strlen (p->type); j < long_ptype; j++)
            g_string_append_c (out, ' ');
          g_string_append_c (out, ' ');

          for (guint j = p->n_star; j < long_star; j++)
            g_string_append_c (out, ' ');
          for (guint j = 0; j < p->n_star; j++)
            g_string_append_c (out, '*');

          g_string_append (out, p->name);

          if (iter->next)
            {
              g_string_append (out, ",\n");
              for (guint j = 0; j < space; j++)
                g_string_append_c (out, ' ');
            }
        }

      g_string_append_c (out, ')');

      if (chunk->post)
        g_string_append_printf (out, " %s", chunk->post);

      g_string_append (out, ";\n");
    }

  return g_string_free (g_steal_pointer (&out), FALSE);
}
