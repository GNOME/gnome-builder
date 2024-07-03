/*
 * ide-fold-regions-private.h
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

#pragma once

#include <gtk/gtk.h>

#include "ide-fold-regions.h"

G_BEGIN_DECLS

typedef struct _IdeFoldRegion
{
  GtkTextMark *begin;
  GtkTextMark *end;
  GtkTextTag *tag;
  guint begin_line;
  guint begin_line_offset;
  guint end_line;
  guint end_line_offset;
} IdeFoldRegion;

IdeFoldRegions      *_ide_fold_regions_new          (void);
void                 _ide_fold_regions_merge        (IdeFoldRegions *self,
                                                     IdeFoldRegions *other,
                                                     GtkTextBuffer  *buffer);
void                 _ide_fold_regions_stash        (IdeFoldRegions *self,
                                                     GtkTextBuffer  *buffer);
const IdeFoldRegion *_ide_fold_regions_find_at_line (IdeFoldRegions *self,
                                                     guint           line);

static inline int
_uint_compare (guint left,
               guint right)
{
  if (left < right)
    return -1;
  else if (left > right)
    return 1;
  else
    return 0;
}

static inline int
_ide_fold_region_compare (const IdeFoldRegion *left,
                          const IdeFoldRegion *right)
{
  if (left->begin_line < right->begin_line)
    return -1;
  else if (left->begin_line > right->begin_line)
    return 1;

  if (left->begin_line_offset < right->begin_line_offset)
    return -1;
  else if (left->begin_line_offset > right->begin_line_offset)
    return 1;

  if (left->end_line < right->end_line)
    return 1;
  else if (left->end_line > right->end_line)
    return -1;

  if (left->end_line_offset < right->end_line_offset)
    return 1;
  else if (left->end_line_offset > right->end_line_offset)
    return -1;

  g_assert (left->begin_line == right->begin_line);
  g_assert (left->begin_line_offset == right->begin_line_offset);
  g_assert (left->end_line == right->end_line);
  g_assert (left->end_line_offset == right->end_line_offset);

  return 0;
}

static inline void
_ide_fold_region_add (IdeFoldRegion *fold_region,
                      GtkTextBuffer *buffer)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (fold_region->begin == NULL);
  g_assert (fold_region->end == NULL);
  g_assert (fold_region->tag == NULL);

  gtk_text_buffer_get_iter_at_line_offset (buffer,
                                           &begin,
                                           fold_region->begin_line,
                                           fold_region->begin_line_offset);
  gtk_text_buffer_get_iter_at_line_offset (buffer,
                                           &end,
                                           fold_region->end_line,
                                           fold_region->end_line_offset);

  fold_region->tag = gtk_text_buffer_create_tag (buffer, NULL, NULL);
  fold_region->begin = gtk_text_buffer_create_mark (buffer, NULL, &begin, TRUE);
  fold_region->end = gtk_text_buffer_create_mark (buffer, NULL, &end, FALSE);

  gtk_text_buffer_apply_tag (buffer, fold_region->tag, &begin, &end);

  g_object_ref (fold_region->tag);
  g_object_ref (fold_region->begin);
  g_object_ref (fold_region->end);
}

static inline void
_ide_fold_region_remove (IdeFoldRegion *fold_region,
                         GtkTextBuffer *buffer)
{
  g_assert (fold_region->begin != NULL);
  g_assert (fold_region->end != NULL);
  g_assert (fold_region->tag != NULL);

  gtk_text_tag_table_remove (gtk_text_buffer_get_tag_table (buffer), fold_region->tag);
  gtk_text_buffer_delete_mark (buffer, fold_region->end);
  gtk_text_buffer_delete_mark (buffer, fold_region->begin);

  g_clear_object (&fold_region->tag);
  g_clear_object (&fold_region->begin);
  g_clear_object (&fold_region->end);
}

G_END_DECLS
