/* c-parse-helper.c
 *
 * Copyright 2014-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "c-parser"

#include <string.h>

#include "c-parse-helper.h"

void
parameter_free (Parameter *p)
{
  if (p)
    {
      g_clear_pointer (&p->name, g_free);
      g_clear_pointer (&p->type, g_free);
      g_slice_free (Parameter, p);
    }
}

Parameter *
parameter_copy (const Parameter *src)
{
  Parameter *copy;

  copy = g_slice_new0 (Parameter);
  copy->name = g_strdup (src->name);
  copy->type = g_strdup (src->type);
  copy->ellipsis = src->ellipsis;
  copy->n_star = src->n_star;

  return copy;
}

gboolean
parameter_validate (Parameter *param)
{
  const gchar *tmp;

  if (param->ellipsis)
    return TRUE;

  if (!param->name || !param->type)
    return FALSE;

  for (tmp = param->name; *tmp; tmp = g_utf8_next_char (tmp))
    {
      gunichar ch = g_utf8_get_char (tmp);

      switch (ch) {
      case '_':
      case '[':
      case ']':
        continue;

      default:
        if (g_unichar_isalnum (ch))
          continue;
        break;
      }

      return FALSE;
    }

  for (tmp = param->type; *tmp; tmp = g_utf8_next_char (tmp))
    {
      gunichar ch = g_utf8_get_char (tmp);

      switch (ch) {
      case '*':
      case ' ':
      case '_':
        continue;

      default:
        if (g_unichar_isalnum (ch))
          continue;
        break;
      }

      return FALSE;
    }

  return TRUE;
}

static void
parameter_compute (Parameter *param)
{
  const gchar *tmp;
  gchar *rev;
  guint n_star = 0;

  rev = g_utf8_strreverse (param->type, -1);

  for (tmp = rev; tmp; tmp = g_utf8_next_char (tmp))
    {
      switch (g_utf8_get_char (tmp))
        {
        case ' ':
          break;

        case '*':
          n_star++;
          break;

        default:
          if (n_star)
            {
              gchar *cleaned;

              cleaned = g_strstrip (g_utf8_strreverse (tmp, -1));
              g_free (param->type);
              param->type = cleaned;
            }
          goto finish;
        }
    }

finish:
  param->n_star = n_star;

  g_free (rev);
}

GSList *
parse_parameters (const gchar *text)
{
  GSList *ret = NULL;
  gchar **parts = NULL;
  guint i;

  parts = g_strsplit (text, ",", 0);

  for (i = 0; parts [i]; i++)
    {
      const gchar *tmp;
      const gchar *word;

      word = g_strstrip (parts [i]);

      if (!*word)
        goto failure;

      if (g_strcmp0 (word, "...") == 0)
        {
          Parameter param = { NULL, NULL, TRUE };
          ret = g_slist_append (ret, parameter_copy (&param));
          continue;
        }

      /*
       * Check that each word only contains valid characters for a
       * parameter list.
       */
      for (tmp = word; *tmp; tmp = g_utf8_next_char (tmp))
        {
          gunichar ch;

          ch = g_utf8_get_char (tmp);

          switch (ch)
            {
            case '\t':
            case ' ':
            case '*':
            case '_':
            case '[':
            case ']':
              break;

            default:
              if (g_unichar_isalnum (ch))
                break;

              goto failure;
            }
        }

      if (strchr (word, '[') && strchr (word, ']'))
        {
          /*
           * TODO: Special case parsing of parameters that have [] after the
           *       name. Such as "char foo[12]" or "char foo[static 12]".
           */
        }
      else
        {
          const gchar *name_sep;
          Parameter param = { 0 };
          gboolean success = FALSE;
          gchar *reversed = NULL;
          gchar *name_rev = NULL;

          reversed = g_utf8_strreverse (word, -1);
          name_sep = strpbrk (reversed, "\t\n *");

          if (name_sep && *name_sep && *(name_sep + 1))
            {
              name_rev = g_strndup (reversed, name_sep - reversed);

              param.name = g_strstrip (g_utf8_strreverse (name_rev, -1));
              param.type = g_strstrip (g_utf8_strreverse (name_sep, -1));

              parameter_compute (&param);

              if (parameter_validate (&param))
                {
                  ret = g_slist_append (ret, parameter_copy (&param));
                  success = TRUE;
                }

              g_free (param.name);
              g_free (param.type);
              g_free (name_rev);
            }

          g_free (reversed);

          if (success)
            continue;
        }

      goto failure;
    }

  goto cleanup;

failure:
  g_slist_foreach (ret, (GFunc)parameter_free, NULL);
  g_clear_pointer (&ret, g_slist_free);

cleanup:
  g_strfreev (parts);

  return ret;
}
