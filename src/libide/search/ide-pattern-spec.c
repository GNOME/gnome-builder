/* ide-pattern-spec.c
 *
 * Copyright (C) 2015-2021 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-pattern-spec"

#include "config.h"

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <string.h>

#include "ide-pattern-spec.h"

G_DEFINE_BOXED_TYPE (IdePatternSpec, ide_pattern_spec, ide_pattern_spec_ref, ide_pattern_spec_unref)

/**
 * SECTION:ide-pattern-spec
 * @title: IdePatternSpec
 * @short_description: Simple glob-like searching
 *
 * This works similar to #GPatternSpec except the query syntax is different.
 * It tries to match word boundaries, but with matching partial words up
 * to those boundaries. For example, "gtk widg" would match "gtk_widget_show".
 * Word boundaries include '_' and ' '. If any character is uppercase, then
 * case sensitivity is used.
 */

struct _IdePatternSpec
{
  volatile int    ref_count;
  char           *needle;
  char          **parts;
  guint           case_sensitive : 1;
};

#ifdef G_OS_WIN32
/* A fallback for missing strcasestr() on Windows. This is not in any way
 * optimized, but at least it supports something resembling UTF-8.
 */
static char *
strcasestr (const char *haystack,
            const char *needle)
{
  g_autofree char *haystack_folded = g_utf8_casefold (haystack, -1);
  g_autofree char *needle_folded = g_utf8_casefold (needle, -1);
  const char *pos;
  gsize n_chars = 0;

  pos = strstr (haystack_folded, needle_folded);

  if (pos == NULL)
    return NULL;

  for (const char *iter = haystack_folded;
       *iter != '\0';
       iter = g_utf8_next_char (iter))
    {
      if (iter >= pos)
        break;
      n_chars++;
    }

  return g_utf8_offset_to_pointer (haystack, n_chars);
}
#endif

IdePatternSpec *
ide_pattern_spec_new (const char *needle)
{
  IdePatternSpec *self;
  const char *tmp;

  if (needle == NULL)
    needle = "";

  self = g_slice_new0 (IdePatternSpec);
  self->ref_count = 1;
  self->needle = g_strdup (needle);
  self->parts = g_strsplit (needle, " ", 0);
  self->case_sensitive = FALSE;

  for (tmp = needle; *tmp; tmp = g_utf8_next_char (tmp))
    {
      if (g_unichar_isupper (g_utf8_get_char (tmp)))
        {
          self->case_sensitive = TRUE;
          break;
        }
    }

  return self;
}

const char *
ide_pattern_spec_get_text (IdePatternSpec *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->needle;
}

static void
ide_pattern_spec_free (IdePatternSpec *self)
{
  g_clear_pointer (&self->parts, g_strfreev);
  g_clear_pointer (&self->needle, g_free);
  g_slice_free (IdePatternSpec, self);
}

static inline gboolean
is_word_break (gunichar ch)
{
  return (ch == ' ' || ch == '_' || ch == '-' || ch == '.');
}

static const char *
next_word_start (const char *haystack)
{
  for (; *haystack; haystack = g_utf8_next_char (haystack))
    {
      gunichar ch = g_utf8_get_char (haystack);

      if (is_word_break (ch))
        break;
    }

  for (; *haystack; haystack = g_utf8_next_char (haystack))
    {
      gunichar ch = g_utf8_get_char (haystack);

      if (is_word_break (ch))
        continue;

      break;
    }

  g_return_val_if_fail (*haystack == '\0' || !is_word_break (*haystack), NULL);

  return haystack;
}

gboolean
ide_pattern_spec_match (IdePatternSpec *self,
                        const char     *haystack)
{
  gsize i;

  if (self == NULL || haystack == NULL)
    return FALSE;

  for (i = 0; (haystack != NULL) && self->parts [i]; i++)
    {
      if (self->parts [i][0] == '\0')
        continue;

      if (self->case_sensitive)
        haystack = strstr (haystack, self->parts [i]);
      else
        haystack = strcasestr (haystack, self->parts [i]);

      if (haystack == NULL)
        return FALSE;

      if (self->parts [i + 1] != NULL)
        haystack = next_word_start (haystack + strlen (self->parts [i]));
    }

  return TRUE;
}

IdePatternSpec *
ide_pattern_spec_ref (IdePatternSpec *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_pattern_spec_unref (IdePatternSpec *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_pattern_spec_free (self);
}
