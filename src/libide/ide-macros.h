/* ide-macros.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <glib-object.h>
#include <string.h>

G_BEGIN_DECLS

#define ide_clear_weak_pointer(ptr) \
  (*(ptr) ? (g_object_remove_weak_pointer((GObject*)*(ptr), (gpointer*)ptr),*(ptr)=NULL,1) : 0)

#define ide_set_weak_pointer(ptr,obj) \
  ((obj!=*(ptr))?(ide_clear_weak_pointer(ptr),*(ptr)=obj,((obj)?g_object_add_weak_pointer((GObject*)obj,(gpointer*)ptr),NULL:NULL),1):0)

#define ide_clear_signal_handler(obj,ptr) \
  G_STMT_START { \
    if (*(ptr) != 0) { \
      g_signal_handler_disconnect((obj), *(ptr)); \
      *(ptr) = 0; \
    } \
  } G_STMT_END

/* strlen() generally gets hoisted out automatically */
#define IDE_LITERAL_LENGTH(s) (strlen(s))

#if __GNUC__ >= 7
# define IDE_FALLTHROUGH __attribute__((fallthrough))
#else
# define IDE_FALLTHROUGH
#endif

static inline gboolean
ide_str_empty0 (const gchar *str)
{
  return (str == NULL) || (str[0] == '\0');
}

static inline gboolean
ide_str_equal0 (gconstpointer a,
                gconstpointer b)
{
  return (g_strcmp0 ((const gchar *)a, (const gchar *)b) == 0);
}

G_END_DECLS
