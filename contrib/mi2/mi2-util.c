/* mi2-util.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include "mi2-util.h"

gchar *
mi2_util_parse_string (const gchar  *line,
                       const gchar **endptr)
{
  g_autoptr(GString) str = NULL;

  g_return_val_if_fail (line != NULL, NULL);
  g_return_val_if_fail (line[0] == '"', NULL);

  str = g_string_new (NULL);

  for (++line; *line; line = g_utf8_next_char (line))
    {
      gunichar ch = g_utf8_get_char (line);

      if (ch == '"')
        break;

      /* Handle escape characters */
      if (ch == '\\')
        {
          line = g_utf8_next_char (line);
          if (!*line)
            goto failure;
          ch = g_utf8_get_char (line);

          switch (ch)
            {
            case 'n':
              g_string_append (str, "\n");
              break;

            case 't':
              g_string_append (str, "\t");
              break;

            default:
              g_string_append_unichar (str, ch);
              break;
            }

          continue;
        }

      g_string_append_unichar (str, ch);
    }

  if (*line == '"')
    line++;

  if (endptr)
    *endptr = line;

  return g_string_free (g_steal_pointer (&str), FALSE);

failure:
  g_warning ("Failed to parse string");

  if (endptr)
    *endptr = NULL;

  return NULL;
}

gchar *
mi2_util_parse_word (const gchar  *line,
                     const gchar **endptr)
{
  const gchar *begin = line;
  gchar *ret;

  g_return_val_if_fail (line != NULL, NULL);

  for (; *line; line = g_utf8_next_char (line))
    {
      gunichar ch = g_utf8_get_char (line);

      if (ch == ',' || ch == '=' || g_unichar_isspace (ch))
        break;
    }

  ret = g_strndup (begin, line - begin);

  if (endptr)
    *endptr = line;

  return ret;
}

GVariant *
mi2_util_parse_record (const gchar  *line,
                       const gchar **endptr)
{
  GVariantDict dict;

  g_return_val_if_fail (line != NULL, NULL);

  g_variant_dict_init (&dict, NULL);

  /* move past { if we aren't starting from inside {} */
  if (*line == '{')
    line++;

  while (*line && *line != '}')
    {
      g_autofree gchar *key = NULL;

      if (*line == ',')
        line++;

      if (!(key = mi2_util_parse_word (line, &line)))
        goto failure;

      if (*line == '=')
        line++;

      if (*line == '"')
        {
          g_autofree gchar *value = NULL;

          if (!(value = mi2_util_parse_string (line, &line)))
            goto failure;

          g_variant_dict_insert (&dict, key, "s", value);
        }
      else if (*line == '{')
        {
          g_autoptr(GVariant) v = NULL;

          if (!(v = mi2_util_parse_record (line, &line)))
            goto failure;

          g_variant_dict_insert_value (&dict, key, v);
        }
      else if (*line == '[')
        {
          g_autoptr(GVariant) ar = NULL;

          if (!(ar = mi2_util_parse_list (line, &line)))
            goto failure;

          g_variant_dict_insert_value (&dict, key, ar);
        }
      else
        goto failure;

      if (*line == ',')
        line++;
    }

  if (*line == '}')
    line++;

  if (endptr)
    *endptr = line;

  return g_variant_ref_sink (g_variant_dict_end (&dict));

failure:
  g_warning ("Failed to parse record");
  g_variant_dict_clear (&dict);
  if (endptr)
    *endptr = NULL;
  return NULL;
}

GVariant *
mi2_util_parse_list (const gchar  *line,
                     const gchar **endptr)
{
  GVariantBuilder builder;

  g_return_val_if_fail (line != NULL, NULL);
  g_return_val_if_fail (*line == '[', NULL);

  /* move past [ */
  line++;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

  if (*line != ']')
    {
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("v"));

      while (*line != ']')
        {
          if (*line == '"')
            {
              g_autofree gchar *value = NULL;

              if (!(value = mi2_util_parse_string (line, &line)))
                goto failure;

              g_variant_builder_add (&builder, "s", value);
            }
          else if (*line == '{')
            {
              g_autoptr(GVariant) v = NULL;

              if (!(v = mi2_util_parse_record (line, &line)))
                goto failure;

              g_variant_builder_add_value (&builder, v);
            }
          else if (*line == '[')
            {
              g_autoptr(GVariant) ar = NULL;

              if (!(ar = mi2_util_parse_list (line, &line)))
                goto failure;

              g_variant_builder_add_value (&builder, ar);
            }
          else
            goto failure;


          if (*line == ',')
            line++;
        }

      g_variant_builder_close (&builder);
    }

  g_assert (*line == ']');

  line++;

  if (endptr)
    *endptr = line;

  return g_variant_ref_sink (g_variant_builder_end (&builder));

failure:
  g_warning ("Failed to parse list");
  g_variant_builder_clear (&builder);
  if (endptr)
    *endptr = NULL;
  return NULL;
}
