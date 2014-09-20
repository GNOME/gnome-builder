/* c-parse-helper.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "c-parser"

#include "gb-log.h"

#include "c-parse-helper.h"

void
parameter_free (Parameter *p)
{
  if (p)
    {
      g_free (p->name);
      g_free (p->type);
      g_free (p);
    }
}

Parameter *
parameter_copy (const Parameter *src)
{
  Parameter *copy;

  copy = g_new0 (Parameter, 1);
  copy->name = g_strdup (src->name);
  copy->type = g_strdup (src->type);
  copy->ellipsis = src->ellipsis;

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
        if (!g_unichar_isalnum (ch))
          return FALSE;
      }
    }

  for (tmp = param->type; *tmp; tmp = g_utf8_next_char (tmp))
    {
      gunichar ch = g_utf8_get_char (tmp);

      switch (ch) {
      case '*':
      case '[':
      case ']':
        continue;

      default:
        if (!g_unichar_isalnum (ch))
          return FALSE;
      }
    }

  return TRUE;
}

GSList *
parse_parameters (const gchar *text)
{
  GSList *ret = NULL;
  gchar **parts = NULL;
  guint i;

  ENTRY;

  parts = g_strsplit (text, ",", 0);

  for (i = 0; parts [i]; i++)
    {
      Parameter param = { 0 };
      const gchar *tmp;
      const gchar *word;
      gboolean success = FALSE;
      gchar *reversed = NULL;

      word = g_strstrip (parts [i]);

      if (!*word)
        GOTO (failure);

      if (g_strcmp0 (word, "...") == 0)
        {
          param.ellipsis = TRUE;
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

              GOTO (failure);
            }
        }

      /*
       * Extract the variable name and type. We do this by reversing the
       * string so that it is more convenient to walk. After we get the
       * name, the rest should be the type info.
       */
      reversed = g_utf8_strreverse (word, -1);
      for (tmp = reversed; *tmp; tmp = g_utf8_next_char (tmp))
        {
          gunichar ch = g_utf8_get_char (tmp);

          /*
           * If we are past our alnum characters that are valid for a name,
           * go ahead and add the parameter to the list. Validate it first
           * though so we only continue if we are absolutely sure it's okay.
           *
           * Note that the name can have [] in it like so:
           *   "void foo (char name[32]);"
           */
          if (!g_unichar_isalnum (ch) && (ch != '[') && (ch != ']'))
            {
              gchar *name;
              gchar *type;

              if (tmp == reversed)
                GOTO (failure);

              name = g_strndup (reversed, tmp - 1 - reversed);
              type = g_strdup (tmp - 1);

              param.name = g_strstrip (g_utf8_strreverse (name, -1));
              param.type = g_strstrip (g_utf8_strreverse (type, -1));

              if (parameter_validate (&param))
                {
                  ret = g_slist_append (ret, parameter_copy (&param));
                  success = TRUE;
                }

              g_print ("name: %s type: %s\n", name, type);

              g_free (param.name);
              g_free (param.type);
              g_free (name);
              g_free (type);
            }
        }

      g_free (reversed);

      if (success)
        continue;

      GOTO (failure);
    }

  GOTO (cleanup);

failure:
  g_slist_foreach (ret, (GFunc)parameter_free, NULL);
  g_clear_pointer (&ret, g_slist_free);

cleanup:
  g_strfreev (parts);

    {
      GSList *iter;

      for (iter = ret; iter; iter = iter->next)
        {
          Parameter *p = iter->data;

          g_print ("PARAM: Type(%s) Name(%s)\n", p->type, p->name);
        }
    }

  RETURN (ret);
}
