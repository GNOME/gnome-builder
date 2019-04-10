/* line-cache.c
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

#define G_LOG_DOMAIN "line-cache"

#include <stdlib.h>

#include "line-cache.h"

struct _LineCache
{
  GArray *lines;
};

LineCache *
line_cache_new (void)
{
  LineCache *self;

  self = g_slice_new0 (LineCache);
  self->lines = g_array_new (FALSE, FALSE, sizeof (LineEntry));

  return self;
}

void
line_cache_free (LineCache *self)
{
  if (self != NULL)
    {
      g_clear_pointer (&self->lines, g_array_unref);
      g_slice_free (LineCache, self);
    }
}

static LineEntry *
get_entry (const LineCache *self,
           gint             line)
{
  LineEntry empty = {0};
  gint i;

  /* Our access pattern is usally either the head (0) or somewhere
   * near the end (ideally the end). So we try to access the elements
   * in that order to avoid needless spinning.
   */

  if (line == 0)
    {
      if (self->lines->len == 0 ||
          g_array_index (self->lines, LineEntry, 0).line != 0)
        g_array_insert_val (self->lines, 0, empty);

      return &g_array_index (self->lines, LineEntry, 0);
    }

  for (i = self->lines->len - 1; i >= 0; i--)
    {
      LineEntry *entry = &g_array_index (self->lines, LineEntry, i);

      if (entry->line == line)
        return entry;

      if (entry->line < line)
        break;
    }

  if (i < 0 || i < self->lines->len)
    i++;

  g_assert (i >= 0);
  g_assert (i <= self->lines->len);

  empty.line = line;
  g_array_insert_val (self->lines, i, empty);
  return &g_array_index (self->lines, LineEntry, i);
}

void
line_cache_mark_range (LineCache *self,
                       gint       start_line,
                       gint       end_line,
                       LineMark   mark)
{
  g_assert (self != NULL);
  g_assert (end_line >= start_line);
  g_assert (mark != 0);

  do
    {
      LineEntry *entry = get_entry (self, start_line);
      entry->mark |= mark;
      start_line++;
    }
  while (start_line < end_line);
}

static gint
compare_by_line (gconstpointer a,
                 gconstpointer b)
{
  const gint *line = a;
  const LineEntry *entry = b;

  return *line - entry->line;
}

LineMark
line_cache_get_mark (const LineCache *self,
                     gint             line)
{

  const LineEntry *ret;

  ret = bsearch (&line, (gconstpointer)self->lines->data,
                 self->lines->len, sizeof (LineEntry),
                 (GCompareFunc)compare_by_line);

  return ret ? ret->mark : 0;
}

static const LineEntry *
line_cache_first_in_range (const LineCache *self,
                           gint             start_line,
                           gint             end_line)
{
  gint L;
  gint R;

  if (self->lines->len == 0)
    return NULL;

  L = 0;
  R = self->lines->len - 1;

  while (L <= R)
    {
      gint m = (L + R) / 2;
      const LineEntry *entry = &g_array_index (self->lines, LineEntry, m);

      if (entry->line < start_line)
        {
          L = m + 1;
          continue;
        }
      else if (entry->line > end_line)
        {
          R = m - 1;
          continue;
        }

      for (gint p = m; p >= 0; p--)
        {
          const LineEntry *prev = &g_array_index (self->lines, LineEntry, p);

          if (prev->line >= start_line)
            entry = prev;
        }

      return entry;
    }

  return NULL;
}

void
line_cache_foreach_in_range (const LineCache *self,
                             gint             start_line,
                             gint             end_line,
                             GFunc            callback,
                             gpointer         user_data)
{
  const LineEntry *base;
  const LineEntry *end;
  const LineEntry *entry = NULL;

  g_assert (self != NULL);
  g_assert (self->lines != NULL);

  if (self->lines->len == 0)
    return;

  base = &g_array_index (self->lines, LineEntry, 0);
  end = base + self->lines->len;

  if ((entry = line_cache_first_in_range (self, start_line, end_line)))
    {
      for (; entry < end && entry->line <= end_line; entry++)
        callback ((gpointer)entry, user_data);
    }
}

GVariant *
line_cache_to_variant (const LineCache *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_variant_take_ref (g_variant_new_fixed_array (G_VARIANT_TYPE ("u"),
                                                        (gconstpointer)self->lines->data,
                                                        self->lines->len,
                                                        sizeof (LineEntry)));
}

LineCache *
line_cache_new_from_variant (GVariant *variant)
{
  LineCache *self;

  self = line_cache_new ();

  if (variant != NULL)
    {
      gconstpointer base;
      gsize n_elements = 0;

      base = g_variant_get_fixed_array (variant, &n_elements, sizeof (LineEntry));

      if (n_elements > 0 && n_elements < G_MAXINT)
        g_array_append_vals (self->lines, base, n_elements);
    }

  return g_steal_pointer (&self);
}
