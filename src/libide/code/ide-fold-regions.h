/*
 * ide-fold-regions.h
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

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_FOLD_REGION_FLAGS    (ide_fold_region_flags_get_type())
#define IDE_TYPE_FOLD_REGIONS         (ide_fold_regions_get_type())
#define IDE_TYPE_FOLD_REGIONS_BUILDER (ide_fold_regions_builder_get_type())

typedef struct _IdeFoldRegionsBuilder IdeFoldRegionsBuilder;

typedef enum _IdeFoldRegionFlags
{
  IDE_FOLD_REGION_FLAGS_NONE          = 0,
  IDE_FOLD_REGION_FLAGS_STARTS_REGION = 1 << 0,
  IDE_FOLD_REGION_FLAGS_IN_REGION     = 1 << 1,
  IDE_FOLD_REGION_FLAGS_ENDS_REGION   = 1 << 2,
} IdeFoldRegionFlags;

typedef void (*IdeFoldRegionsForeachFunc) (guint              line,
                                           IdeFoldRegionFlags flags,
                                           gpointer           user_data);

IDE_AVAILABLE_IN_47
G_DECLARE_FINAL_TYPE (IdeFoldRegions, ide_fold_regions, IDE, FOLD_REGIONS, GObject)

IDE_AVAILABLE_IN_47
GType                  ide_fold_region_flags_get_type    (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_47
gboolean               ide_fold_regions_is_empty         (IdeFoldRegions            *self);
IDE_AVAILABLE_IN_47
void                   ide_fold_regions_foreach_in_range (IdeFoldRegions            *self,
                                                          guint                      begin_line,
                                                          guint                      end_line,
                                                          IdeFoldRegionsForeachFunc  foreach_func,
                                                          gpointer                   user_data);
IDE_AVAILABLE_IN_47
GType                  ide_fold_regions_builder_get_type (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_47
IdeFoldRegionsBuilder *ide_fold_regions_builder_new      (GtkTextBuffer             *buffer);
IDE_AVAILABLE_IN_47
IdeFoldRegionsBuilder *ide_fold_regions_builder_copy     (IdeFoldRegionsBuilder     *builder);
IDE_AVAILABLE_IN_47
void                   ide_fold_regions_builder_free     (IdeFoldRegionsBuilder     *builder);
IDE_AVAILABLE_IN_47
void                   ide_fold_regions_builder_add      (IdeFoldRegionsBuilder     *builder,
                                                          guint                      begin_line,
                                                          int                        begin_line_offset,
                                                          guint                      end_line,
                                                          int                        end_line_offset);
IDE_AVAILABLE_IN_47
IdeFoldRegions        *ide_fold_regions_builder_build    (IdeFoldRegionsBuilder     *builder);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeFoldRegionsBuilder, ide_fold_regions_builder_free)

G_END_DECLS
