/* gbp-copyright-util.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-copyright-util"

#include "config.h"

#include "gbp-copyright-util.h"

static inline gboolean
is_dash (const char *token)
{
  return token[0] == '-' && token[1] == 0;
}

static inline gboolean
isalnumdigit (char c)
{
  return c >= '0' && c <= '9';
}

static inline gboolean
is_year (const char *token)
{
  return token[0] != 0 && isalnumdigit (token[0]) &&
         token[1] != 0 && isalnumdigit (token[1]) &&
         token[2] != 0 && isalnumdigit (token[2]) &&
         token[3] != 0 && isalnumdigit (token[3]) &&
         token[4] == 0;
}

static char *
replace_copyright_year (const char * const *tokens,
                        guint               n_tokens,
                        const char         *with_year)
{
  g_autoptr(GPtrArray) ar = NULL;
  int dash = -1;

  ar = g_ptr_array_sized_new (n_tokens + 3);
  memcpy (ar->pdata, tokens, sizeof (char *) * n_tokens);
  ar->pdata[n_tokens] = NULL;
  ar->len = n_tokens + 1;

  for (guint i = 0; i < n_tokens; i++)
    {
      if (i > 0 && is_dash (tokens[i]))
        dash = i;
      else if (g_strcmp0 (tokens[i], with_year) == 0)
        return NULL;
    }

  if (dash >= 0)
    {
      if (dash+1 < n_tokens && is_year (tokens[dash+1]))
        g_ptr_array_index (ar, dash+1) = (gpointer)with_year;
      else
        g_ptr_array_insert (ar, dash+1, (gpointer)with_year);
    }
  else
    {
      g_ptr_array_insert (ar, 2, (gpointer)"-");
      g_ptr_array_insert (ar, 3, (gpointer)with_year);

      /* Maybe swallow trailing - like "2022- " */
      if (ar->pdata[4] != NULL && ((char *)ar->pdata[4])[0] == '-')
        ar->pdata[4] = &((char *)ar->pdata[4])[1];
    }

  return g_strjoinv (NULL, (char **)(gpointer)ar->pdata);
}

char *
gbp_update_copyright (const char *input,
                      const char *with_year)
{
  static GRegex *regex;
  g_auto(GStrv) tokens = NULL;
  guint n_tokens;

  if (input == NULL || input[0] == 0)
    return NULL;

  if (regex == NULL)
    regex = g_regex_new ("([0-9]{4})", G_REGEX_OPTIMIZE, 0, NULL);

  tokens = g_regex_split (regex, input, 0);

  /* n_tokens > 2 */
  if (tokens == NULL || tokens[0] == NULL || tokens[1] == NULL)
    return NULL;

  /* Sanity check */
  n_tokens = g_strv_length (tokens);
  if (n_tokens > 6)
    return NULL;

  return replace_copyright_year ((const char * const *)tokens, n_tokens, with_year);
}
