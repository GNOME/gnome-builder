/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/*
 * Static inline GList functions to allow for more aggressive
 * inline of function callbacks in consuming code.
 */
static inline GList *
ide_list_sort_merge (GList            *l1,
                     GList            *l2,
                     GCompareDataFunc  compare_func,
                     gpointer          user_data)
{
  GList list, *l, *lprev;
  gint cmp;

  l = &list;
  lprev = NULL;

  while (l1 && l2)
    {
      cmp = compare_func (l1->data, l2->data, user_data);

      if (cmp <= 0)
        {
          l->next = l1;
          l1 = l1->next;
        }
      else
        {
          l->next = l2;
          l2 = l2->next;
        }
      l = l->next;
      l->prev = lprev;
      lprev = l;
    }
  l->next = l1 ? l1 : l2;
  l->next->prev = l;

  return list.next;
}

static inline GList *
ide_list_sort_with_data (GList            *list,
                         GCompareDataFunc  compare_func,
                         gpointer          user_data)
{
  GList *l1, *l2;

  if (!list)
    return NULL;
  if (!list->next)
    return list;

  l1 = list;
  l2 = list->next;

  while ((l2 = l2->next) != NULL)
    {
      if ((l2 = l2->next) == NULL)
        break;
      l1 = l1->next;
    }
  l2 = l1->next;
  l1->next = NULL;

  return ide_list_sort_merge (ide_list_sort_with_data (list, compare_func, user_data),
                              ide_list_sort_with_data (l2, compare_func, user_data),
                              compare_func,
                              user_data);
}

static inline GList *
ide_list_sort (GList        *list,
               GCompareFunc  compare_func)
{
  return ide_list_sort_with_data (list, (GCompareDataFunc)compare_func, NULL);
}

G_END_DECLS
