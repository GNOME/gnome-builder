/* ide-task.h
 *
 * Copyright 2018 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if !defined (IDE_THREADING_INSIDE) && !defined (IDE_THREADING_COMPILATION)
# error "Only <libide-threading.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_TASK (ide_task_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTask, ide_task, IDE, TASK, GObject)

typedef void (*IdeTaskThreadFunc) (IdeTask      *task,
                                   gpointer      source_object,
                                   gpointer      task_data,
                                   GCancellable *cancellable);

typedef enum
{
  IDE_TASK_KIND_DEFAULT,
  IDE_TASK_KIND_COMPILER,
  IDE_TASK_KIND_INDEXER,
  IDE_TASK_KIND_IO,
  IDE_TASK_KIND_LAST
} IdeTaskKind;

IDE_AVAILABLE_IN_ALL
IdeTask      *ide_task_new                       (gpointer              source_object,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
IDE_AVAILABLE_IN_ALL
void          ide_task_chain                     (IdeTask              *self,
                                                  IdeTask              *other_task);
IDE_AVAILABLE_IN_ALL
GCancellable *ide_task_get_cancellable           (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
gboolean      ide_task_get_completed             (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
IdeTaskKind   ide_task_get_kind                  (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
const gchar  *ide_task_get_name                  (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
gint          ide_task_get_priority              (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
gint          ide_task_get_complete_priority     (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
gpointer      ide_task_get_source_object         (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
gpointer      ide_task_get_source_tag            (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
gpointer      ide_task_get_task_data             (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
gboolean      ide_task_had_error                 (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
gboolean      ide_task_is_valid                  (gpointer              self,
                                                  gpointer              source_object);
IDE_AVAILABLE_IN_ALL
gboolean      ide_task_propagate_boolean         (IdeTask              *self,
                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
gpointer      ide_task_propagate_boxed           (IdeTask              *self,
                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
gssize        ide_task_propagate_int             (IdeTask              *self,
                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
gpointer      ide_task_propagate_object          (IdeTask              *self,
                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
gpointer      ide_task_propagate_pointer         (IdeTask              *self,
                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
void          ide_task_return_boolean            (IdeTask              *self,
                                                  gboolean              result);
IDE_AVAILABLE_IN_ALL
void          ide_task_return_boxed              (IdeTask              *self,
                                                  GType                 result_type,
                                                  gpointer              result);
IDE_AVAILABLE_IN_ALL
void          ide_task_return_error              (IdeTask              *self,
                                                  GError               *error);
IDE_AVAILABLE_IN_ALL
gboolean      ide_task_return_error_if_cancelled (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
void          ide_task_return_int                (IdeTask              *self,
                                                  gssize                result);
IDE_AVAILABLE_IN_ALL
gboolean      ide_task_get_return_on_cancel      (IdeTask              *self);
IDE_AVAILABLE_IN_ALL
void          ide_task_return_new_error          (IdeTask              *self,
                                                  GQuark                error_domain,
                                                  gint                  error_code,
                                                  const gchar          *format,
                                                  ...) G_GNUC_PRINTF (4, 5);
IDE_AVAILABLE_IN_ALL
void          ide_task_return_object             (IdeTask              *self,
                                                  gpointer              instance);
IDE_AVAILABLE_IN_ALL
void          ide_task_return_pointer            (IdeTask              *self,
                                                  gpointer              data,
                                                  GDestroyNotify        destroy);
IDE_AVAILABLE_IN_ALL
void          ide_task_run_in_thread             (IdeTask              *self,
                                                  IdeTaskThreadFunc     thread_func);
IDE_AVAILABLE_IN_ALL
void          ide_task_set_check_cancellable     (IdeTask              *self,
                                                  gboolean              check_cancellable);
IDE_AVAILABLE_IN_ALL
void          ide_task_set_kind                  (IdeTask              *self,
                                                  IdeTaskKind           kind);
IDE_AVAILABLE_IN_ALL
void          ide_task_set_name                  (IdeTask              *self,
                                                  const gchar          *name);
IDE_AVAILABLE_IN_ALL
void          ide_task_set_priority              (IdeTask              *self,
                                                  gint                  priority);
IDE_AVAILABLE_IN_ALL
void          ide_task_set_complete_priority     (IdeTask              *self,
                                                  gint                  complete_priority);
IDE_AVAILABLE_IN_ALL
void          ide_task_set_release_on_propagate  (IdeTask              *self,
                                                  gboolean              release_on_propagate);
IDE_AVAILABLE_IN_ALL
void          ide_task_set_return_on_cancel      (IdeTask              *self,
                                                  gboolean              return_on_cancel);
IDE_AVAILABLE_IN_ALL
void          ide_task_set_source_tag            (IdeTask              *self,
                                                  gpointer              source_tag);
IDE_AVAILABLE_IN_ALL
void          ide_task_set_task_data             (IdeTask              *self,
                                                  gpointer              task_data,
                                                  GDestroyNotify        task_data_destroy);
IDE_AVAILABLE_IN_ALL
void          ide_task_report_new_error          (gpointer              source_object,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              callback_data,
                                                  gpointer              source_tag,
                                                  GQuark                domain,
                                                  gint                  code,
                                                  const gchar          *format,
                                                  ...) G_GNUC_PRINTF (7, 8);

static inline void
ide_task_return_unsupported_error (IdeTask *task)
{
  g_assert (IDE_IS_TASK (task));

  ide_task_return_error (task,
                         g_error_new_literal (G_IO_ERROR,
                                              G_IO_ERROR_NOT_SUPPORTED,
                                              "Not Supported"));
}

#ifdef __GNUC__
# define ide_task_new(self, cancellable, callback, user_data)                      \
  ({                                                                               \
    IdeTask *__ide_task = (ide_task_new) (self, cancellable, callback, user_data); \
    ide_task_set_name (__ide_task, g_intern_static_string (G_STRLOC));             \
    __ide_task;                                                                    \
  })
# define ide_task_set_task_data(t,d,n)                                             \
  G_STMT_START {                                                                   \
    __typeof__((d)) __task_data = (d);                                             \
    (ide_task_set_task_data) ((t),                                                 \
                              __task_data,                                         \
                              0 ? (((n) (__task_data)), (GDestroyNotify)NULL)      \
                                : ((GDestroyNotify)(n)));                          \
  } G_STMT_END
# define ide_task_return_pointer(t,d,n)                                            \
  G_STMT_START {                                                                   \
    __typeof__((d)) __task_return = (d);                                           \
    if ((n) != NULL)                                                               \
      (ide_task_return_pointer) ((t),                                              \
                                 __task_return,                                    \
                                 0 ? (((n) (__task_return)), (GDestroyNotify)NULL) \
                                   : ((GDestroyNotify)(n)));                       \
    else                                                                           \
      (ide_task_return_pointer) ((t), __task_return, NULL);                        \
  } G_STMT_END
#endif

G_END_DECLS
