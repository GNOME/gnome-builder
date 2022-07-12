/* cjhtextregionprivate.h
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __CJH_TEXT_REGION_PRIVATE_H__
#define __CJH_TEXT_REGION_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _CjhTextRegion CjhTextRegion;

typedef struct _CjhTextRegionRun
{
  gsize length;
  gpointer data;
} CjhTextRegionRun;

/*
 * CjhTextRegionForeachFunc:
 * @offset: the offset in characters within the text region
 * @run: the run of text and data pointer
 * @user_data: user data supplied
 *
 * Function callback to iterate through runs within a text region.
 *
 * Returns: %FALSE to coninue iteration, otherwise %TRUE to stop.
 */
typedef gboolean (*CjhTextRegionForeachFunc) (gsize                   offset,
                                              const CjhTextRegionRun *run,
                                              gpointer                user_data);

/*
 * CjhTextRegionJoinFunc:
 *
 * This callback is used to determine if two runs can be joined together.
 * This is useful when you have similar data pointers between two runs
 * and seeing them as one run is irrelevant to the code using the
 * text region.
 *
 * The default calllback for joining will return %FALSE so that no joins
 * may occur.
 *
 * Returns: %TRUE if the runs can be joined; otherwise %FALSE
 */
typedef gboolean (*CjhTextRegionJoinFunc) (gsize                   offset,
                                           const CjhTextRegionRun *left,
                                           const CjhTextRegionRun *right);

/*
 * CjhTextRegionSplitFunc:
 *
 * This function is responsible for splitting a run into two runs.
 * This can happen a delete happens in the middle of a run.
 *
 * By default, @left will contain the run prior to the delete, and
 * @right will contain the run after the delete.
 *
 * You can use the run lengths to determine where the delete was made
 * using @offset which is an absolute offset from the beginning of the
 * region.
 *
 * If you would like to keep a single run after the deletion, then
 * set @right to contain a length of zero and add it's previous
 * length to @left.
 *
 * All the length in @left and @right must be accounted for.
 *
 * This function is useful when using CjhTextRegion as a piecetable
 * where you want to adjust the data pointer to point at a new
 * section of an original or change buffer.
 */
typedef void (*CjhTextRegionSplitFunc)     (gsize                     offset,
                                            const CjhTextRegionRun   *run,
                                            CjhTextRegionRun         *left,
                                            CjhTextRegionRun         *right);

CjhTextRegion *_cjh_text_region_new              (CjhTextRegionJoinFunc     join_func,
                                                  CjhTextRegionSplitFunc    split_func);
void           _cjh_text_region_insert           (CjhTextRegion            *region,
                                                  gsize                     offset,
                                                  gsize                     length,
                                                  gpointer                  data);
void           _cjh_text_region_replace          (CjhTextRegion            *region,
                                                  gsize                     offset,
                                                  gsize                     length,
                                                  gpointer                  data);
void           _cjh_text_region_remove           (CjhTextRegion            *region,
                                                  gsize                     offset,
                                                  gsize                     length);
guint          _cjh_text_region_get_length       (CjhTextRegion            *region);
void           _cjh_text_region_foreach          (CjhTextRegion            *region,
                                                  CjhTextRegionForeachFunc  func,
                                                  gpointer                  user_data);
void           _cjh_text_region_foreach_in_range (CjhTextRegion            *region,
                                                  gsize                     begin,
                                                  gsize                     end,
                                                  CjhTextRegionForeachFunc  func,
                                                  gpointer                  user_data);
void           _cjh_text_region_free             (CjhTextRegion            *region);

G_END_DECLS

#endif /* __CJH_TEXT_REGION_PRIVATE_H__ */
