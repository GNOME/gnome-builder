/* ide-glib.h
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_PTR_ARRAY_CLEAR_FREE_FUNC(ar)                       \
  IDE_PTR_ARRAY_SET_FREE_FUNC(ar, NULL)
#define IDE_PTR_ARRAY_SET_FREE_FUNC(ar, func)                   \
  G_STMT_START {                                                \
    if ((ar) != NULL)                                           \
      g_ptr_array_set_free_func ((ar), (GDestroyNotify)(func)); \
  } G_STMT_END
#define IDE_PTR_ARRAY_STEAL_FULL(arptr)        \
  ({ IDE_PTR_ARRAY_CLEAR_FREE_FUNC (*(arptr)); \
     g_steal_pointer ((arptr)); })

#define ide_strv_empty0(strv) (((strv) == NULL) || ((strv)[0] == NULL))

#define ide_set_string(ptr,str) (ide_take_string((ptr), g_strdup(str)))

static inline gboolean
ide_take_string (gchar **ptr,
                 gchar  *str)
{
  if (*ptr != str)
    {
      g_free (*ptr);
      *ptr = str;
      return TRUE;
    }

  return FALSE;
}

static inline void
ide_clear_string (gchar **ptr)
{
  g_free (*ptr);
  *ptr = NULL;
}

IDE_AVAILABLE_IN_3_30
gboolean     ide_environ_parse                        (const gchar          *pair,
                                                       gchar               **key,
                                                       gchar               **value);
IDE_AVAILABLE_IN_ALL
const gchar *ide_gettext                              (const gchar          *message);
IDE_AVAILABLE_IN_ALL
void         ide_g_task_return_boolean_from_main      (GTask                *task,
                                                       gboolean              value);
IDE_AVAILABLE_IN_ALL
void         ide_g_task_return_int_from_main          (GTask                *task,
                                                       gint                  value);
IDE_AVAILABLE_IN_ALL
void         ide_g_task_return_pointer_from_main      (GTask                *task,
                                                       gpointer              value,
                                                       GDestroyNotify        notify);
IDE_AVAILABLE_IN_ALL
void         ide_g_task_return_error_from_main        (GTask                *task,
                                                       GError               *error);
IDE_AVAILABLE_IN_3_28
gchar       *ide_g_file_get_uncanonical_relative_path (GFile                *file,
                                                       GFile                *other);
IDE_AVAILABLE_IN_3_30
void         ide_g_file_find_with_depth_async         (GFile                *file,
                                                       const gchar          *pattern,
                                                       guint                 max_depth,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
IDE_AVAILABLE_IN_3_28
void         ide_g_file_find_async                    (GFile                *file,
                                                       const gchar          *pattern,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
IDE_AVAILABLE_IN_3_28
GPtrArray   *ide_g_file_find_finish                   (GFile                *file,
                                                       GAsyncResult         *result,
                                                       GError              **error);
IDE_AVAILABLE_IN_3_28
void         ide_g_file_get_children_async            (GFile                *file,
                                                       const gchar          *attributes,
                                                       GFileQueryInfoFlags   flags,
                                                       gint                  io_priority,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
IDE_AVAILABLE_IN_3_28
GPtrArray   *ide_g_file_get_children_finish           (GFile                *file,
                                                       GAsyncResult         *result,
                                                       GError              **error);
IDE_AVAILABLE_IN_3_28
gboolean     ide_g_host_file_get_contents             (const gchar          *path,
                                                       gchar               **contents,
                                                       gsize                *len,
                                                       GError              **error);
IDE_AVAILABLE_IN_3_30
GIcon       *ide_g_content_type_get_symbolic_icon     (const gchar          *content_type);

G_END_DECLS
