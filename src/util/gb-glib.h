/* gb-glib.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GB_GLIB_H
#define GB_GLIB_H

#include <glib-object.h>

G_BEGIN_DECLS

#define gb_clear_weak_pointer(ptr) \
  G_STMT_START { \
    if (*(gpointer *)(ptr)) \
      { \
        g_object_remove_weak_pointer (G_OBJECT (*(GObject**)ptr), \
                                      (gpointer *)(ptr)); \
        *(gpointer *)ptr = NULL; \
      } \
  } G_STMT_END

#define gb_set_weak_pointer(obj, ptr) \
  G_STMT_START { \
    if (obj) \
      { \
        *(gpointer *)ptr = obj; \
        g_object_add_weak_pointer (G_OBJECT (obj), (gpointer *)ptr); \
      } \
  } G_STMT_END

G_END_DECLS

#endif /* GB_GLIB_H */
