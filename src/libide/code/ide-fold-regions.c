/*
 * ide-fold-regions.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "ide-fold-regions-private.h"

struct _IdeFoldRegions
{
  GObject parent_instance;
  GArray *regions;
};

struct _IdeFoldRegionsBuilder
{
  GtkTextBuffer *buffer;
  GArray *regions;
};

G_DEFINE_FINAL_TYPE (IdeFoldRegions, ide_fold_regions, G_TYPE_OBJECT)

static void
clear_func (gpointer data)
{
  IdeFoldRegion *fold_region = data;
  GtkTextBuffer *buffer = NULL;

  if (!buffer && fold_region->begin)
    buffer = gtk_text_mark_get_buffer (fold_region->begin);

  if (!buffer && fold_region->end)
    buffer = gtk_text_mark_get_buffer (fold_region->end);

  if (buffer)
    _ide_fold_region_remove (fold_region, buffer);

  g_clear_object (&fold_region->begin);
  g_clear_object (&fold_region->end);
  g_clear_object (&fold_region->tag);
}

static void
ide_fold_regions_dispose (GObject *object)
{
  IdeFoldRegions *self = (IdeFoldRegions *)object;

  g_clear_pointer (&self->regions, g_array_unref);

  G_OBJECT_CLASS (ide_fold_regions_parent_class)->dispose (object);
}

static void
ide_fold_regions_class_init (IdeFoldRegionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_fold_regions_dispose;
}

static void
ide_fold_regions_init (IdeFoldRegions *self)
{
}

gboolean
ide_fold_regions_is_empty (IdeFoldRegions *self)
{
  return self == NULL || self->regions == NULL || self->regions->len == 0;
}

IdeFoldRegions *
_ide_fold_regions_new (void)
{
  return g_object_new (IDE_TYPE_FOLD_REGIONS, NULL);
}

static IdeFoldRegion *
_ide_fold_regions_peek (IdeFoldRegions *self,
                        guint          *n_regions)
{
  g_assert (IDE_IS_FOLD_REGIONS (self));
  g_assert (n_regions != NULL);

  if (self->regions == NULL || self->regions->len == 0)
    {
      *n_regions = 0;
      return NULL;
    }
  else
    {
      *n_regions = self->regions->len;
      return &g_array_index (self->regions, IdeFoldRegion, 0);
    }
}

void
_ide_fold_regions_merge (IdeFoldRegions *self,
                         IdeFoldRegions *other,
                         GtkTextBuffer  *buffer)
{
  g_autoptr(GArray) freeme = NULL;
  IdeFoldRegion *old;
  IdeFoldRegion *new;
  GArray *regions;
  guint n_old;
  guint n_new;
  guint o = 0;
  guint n = 0;

  g_return_if_fail (IDE_IS_FOLD_REGIONS (self));
  g_return_if_fail (IDE_IS_FOLD_REGIONS (other));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  regions = g_array_new (FALSE, FALSE, sizeof (IdeFoldRegion));
  g_array_set_clear_func (regions, clear_func);

  old = _ide_fold_regions_peek (self, &n_old);
  new = _ide_fold_regions_peek (other, &n_new);

  for (; o < n_old; o++)
    {
      IdeFoldRegion *oldr = &old[o];

      g_assert (oldr->tag != NULL);
      g_assert (oldr->begin != NULL);
      g_assert (oldr->end != NULL);

      while (n < n_new)
        {
          IdeFoldRegion *newr = &new[n];
          int cmpval = _ide_fold_region_compare (oldr, newr);

          g_assert (newr->tag == NULL);
          g_assert (newr->begin == NULL);
          g_assert (newr->end == NULL);

          if (cmpval < 0)
            {
              _ide_fold_region_remove (oldr, buffer);
              break;
            }

          if (cmpval == 0)
            {
              IdeFoldRegion copy = *newr;

              copy.tag = g_steal_pointer (&oldr->tag);
              copy.begin = g_steal_pointer (&oldr->begin);
              copy.end = g_steal_pointer (&oldr->end);
              g_array_append_val (regions, copy);
              n++;
              break;
            }

          if (cmpval > 0)
            {
              IdeFoldRegion add = *newr;
              _ide_fold_region_add (&add, buffer);
              g_array_append_val (regions, add);
              n++;
              continue;
            }

          g_assert_not_reached ();
        }
    }

  while (n < n_new)
    {
      IdeFoldRegion add = new[n];
      _ide_fold_region_add (&add, buffer);
      g_array_append_val (regions, add);
      n++;
    }

  freeme = g_steal_pointer (&self->regions);
  self->regions = g_steal_pointer (&regions);
}

void
_ide_fold_regions_stash (IdeFoldRegions *self,
                         GtkTextBuffer  *buffer)
{
  g_return_if_fail (IDE_IS_FOLD_REGIONS (self));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (self->regions == NULL)
    return;

  for (guint i = 0; i < self->regions->len; i++)
    {
      IdeFoldRegion *fold_region = &g_array_index (self->regions, IdeFoldRegion, i);
      GtkTextIter begin;
      GtkTextIter end;

      g_assert (fold_region->begin != NULL);
      g_assert (fold_region->end != NULL);
      g_assert (fold_region->tag != NULL);

      gtk_text_buffer_get_iter_at_mark (buffer, &begin, fold_region->begin);
      gtk_text_buffer_get_iter_at_mark (buffer, &end, fold_region->end);

      fold_region->begin_line = gtk_text_iter_get_line (&begin);
      fold_region->begin_line_offset = gtk_text_iter_get_line_offset (&begin);

      fold_region->end_line = gtk_text_iter_get_line (&end);
      fold_region->end_line_offset = gtk_text_iter_get_line_offset (&end);
    }
}

/**
 * ide_fold_regions_foreach_in_range:
 * @self: a #IdeFoldRegions
 * @begin_line: the first line to accumulate changes for
 * @end_line: the last line to accumulate changes for
 * @foreach_func: (scope call): the callback
 * @user_data: closure data for @foreach_func
 *
 * Calls @foreach_func for every line between begin_line and
 * end_line that has flags to be applied.
 */
void
ide_fold_regions_foreach_in_range (IdeFoldRegions            *self,
                                   guint                      begin_line,
                                   guint                      end_line,
                                   IdeFoldRegionsForeachFunc  foreach_func,
                                   gpointer                   user_data)
{
  g_autofree guint8 *freeme = NULL;
  guint8 *flags;

  g_return_if_fail (IDE_IS_FOLD_REGIONS (self));
  g_return_if_fail (foreach_func != NULL);
  g_return_if_fail (end_line >= begin_line);

  if (self->regions == NULL)
    return;

  /* Try to use stack for compiling state to avoid allocs */
  if (end_line - begin_line < 1024)
    flags = g_alloca0 (end_line - begin_line + 1);
  else
    flags = freeme = g_new0 (guint8, end_line - begin_line + 1);

  /* Now compile our flags for each line working through regions.
   * One could potentially bsearch() to find the starting position
   * but given the size of these it's unlikely to be worth it.
   */
  for (guint i = 0; i < self->regions->len; i++)
    {
      const IdeFoldRegion *region = &g_array_index (self->regions, IdeFoldRegion, i);

      /* If this ends before our range we care about, skip */
      if (region->end_line < begin_line)
        continue;

      /* If we reached something after what we care about, we're done */
      if (region->begin_line > end_line)
        break;

      /* Mark the start flag if it falls in this range */
      if (region->begin_line >= begin_line)
        {
          g_assert (region->begin_line <= end_line);

          flags[region->begin_line - begin_line] |= IDE_FOLD_REGION_FLAGS_STARTS_REGION;
        }

      /* Mark the end flag if it falls in this range */
      if (region->end_line <= end_line)
        {
          g_assert (region->end_line >= begin_line);

          flags[region->end_line - begin_line] |= IDE_FOLD_REGION_FLAGS_ENDS_REGION;
        }

      for (guint l = MAX (begin_line, region->begin_line + 1);
           l < MIN (end_line + 1, region->end_line);
           l++)
        {
          g_assert (l >= begin_line);
          g_assert (l <= end_line);

          flags[l - begin_line] |= IDE_FOLD_REGION_FLAGS_IN_REGION;
        }
    }

  for (guint line = begin_line; line <= end_line; line++)
    foreach_func (line, flags[line-begin_line], user_data);
}

G_DEFINE_BOXED_TYPE (IdeFoldRegionsBuilder,
                     ide_fold_regions_builder,
                     ide_fold_regions_builder_copy,
                     ide_fold_regions_builder_free)

IdeFoldRegionsBuilder *
ide_fold_regions_builder_new (GtkTextBuffer *buffer)
{
  IdeFoldRegionsBuilder *builder;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  builder = g_new0 (IdeFoldRegionsBuilder, 1);
  builder->buffer = g_object_ref (buffer);

  return builder;
}

IdeFoldRegionsBuilder *
ide_fold_regions_builder_copy (IdeFoldRegionsBuilder *builder)
{
  IdeFoldRegionsBuilder *copy;

  if (builder == NULL)
    return NULL;

  copy = ide_fold_regions_builder_new (builder->buffer);

  if (builder->regions != NULL && builder->regions->len > 0)
    {
      copy->regions = g_array_sized_new (FALSE,
                                         FALSE,
                                         sizeof (IdeFoldRegion),
                                         builder->regions->len);
      g_array_set_size (copy->regions, builder->regions->len);
      memcpy (copy->regions->data,
              builder->regions->data,
              sizeof (IdeFoldRegion) * builder->regions->len);
    }

  return copy;
}

void
ide_fold_regions_builder_free (IdeFoldRegionsBuilder *builder)
{
  if (builder != NULL)
    {
      g_clear_object (&builder->buffer);
      g_clear_pointer (&builder->regions, g_array_unref);
      g_free (builder);
    }
}

static inline void
_swap_int (int *a,
           int *b)
{
  int t = *a;
  *a = *b;
  *b = t;
}

static inline void
_swap_uint (guint *a,
            guint *b)
{
  guint t = *a;
  *a = *b;
  *b = t;
}

static guint
find_eol_offset (GtkTextBuffer *buffer,
                 guint          line)
{
  GtkTextIter iter;

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);

  while (!gtk_text_iter_ends_line (&iter))
    gtk_text_iter_forward_char (&iter);

  return gtk_text_iter_get_line_offset (&iter);
}

void
ide_fold_regions_builder_add (IdeFoldRegionsBuilder *builder,
                              guint                  begin_line,
                              int                    begin_line_offset,
                              guint                  end_line,
                              int                    end_line_offset)
{
  IdeFoldRegion region = {0};

  g_return_if_fail (builder != NULL);

  if (begin_line_offset < 0)
    begin_line_offset = find_eol_offset (builder->buffer, begin_line);

  if (end_line_offset < 0)
    end_line_offset = find_eol_offset (builder->buffer, end_line);

  if (begin_line == end_line &&
      begin_line_offset == end_line_offset)
    return;

  if (end_line < begin_line ||
      (end_line == begin_line && end_line_offset < begin_line_offset))
    {
      _swap_uint (&begin_line, &end_line);
      _swap_int (&begin_line_offset, &end_line_offset);
    }

  region.begin_line = begin_line;
  region.begin_line_offset = begin_line_offset;
  region.end_line = end_line;
  region.end_line_offset = end_line_offset;

  if (builder->regions == NULL)
    builder->regions = g_array_new (FALSE, FALSE, sizeof (IdeFoldRegion));

  g_array_append_val (builder->regions, region);
}

/**
 * ide_fold_regions_builder_build:
 * @builder: a #IdeFoldRegions
 *
 * Builds an #IdeFoldRegions from the builder and resets the
 * builder to the initial state.
 *
 * Returns: (transfer full): an #IdeFoldRegions
 */
IdeFoldRegions *
ide_fold_regions_builder_build (IdeFoldRegionsBuilder *builder)
{
  IdeFoldRegions *self;

  g_return_val_if_fail (builder != NULL, NULL);

  self = g_object_new (IDE_TYPE_FOLD_REGIONS, NULL);

  g_assert (self->regions == NULL);

  if ((self->regions = g_steal_pointer (&builder->regions)))
    {
      g_array_sort (self->regions, (GCompareFunc)_ide_fold_region_compare);
      g_array_set_clear_func (self->regions, clear_func);
    }

  return self;
}

const IdeFoldRegion *
_ide_fold_regions_find_at_line (IdeFoldRegions *self,
                                guint           line)
{
  if (self == NULL || self->regions == NULL)
    return NULL;

  for (guint i = 0; i < self->regions->len; i++)
    {
      const IdeFoldRegion *region = &g_array_index (self->regions, IdeFoldRegion, i);

      if (region->begin_line == line)
        return region;
    }

  return NULL;
}

G_DEFINE_FLAGS_TYPE (IdeFoldRegionFlags, ide_fold_region_flags,
                     G_DEFINE_ENUM_VALUE (IDE_FOLD_REGION_FLAGS_NONE, "none"),
                     G_DEFINE_ENUM_VALUE (IDE_FOLD_REGION_FLAGS_STARTS_REGION, "starts-region"),
                     G_DEFINE_ENUM_VALUE (IDE_FOLD_REGION_FLAGS_ENDS_REGION, "ends-region"),
                     G_DEFINE_ENUM_VALUE (IDE_FOLD_REGION_FLAGS_IN_REGION, "in-region"))
