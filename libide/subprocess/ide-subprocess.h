/* ide-subprocess.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_SUBPROCESS_H
#define IDE_SUBPROCESS_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_SUBPROCESS (ide_subprocess_get_type())

G_DECLARE_INTERFACE (IdeSubprocess, ide_subprocess, IDE, SUBPROCESS, GObject)

struct _IdeSubprocessInterface
{
  GTypeInterface parent_interface;

  const gchar   *(*get_identifier)          (IdeSubprocess        *self);
  GInputStream  *(*get_stdout_pipe)         (IdeSubprocess        *self);
  GInputStream  *(*get_stderr_pipe)         (IdeSubprocess        *self);
  GOutputStream *(*get_stdin_pipe)          (IdeSubprocess        *self);
  gboolean       (*wait)                    (IdeSubprocess        *self,
                                             GCancellable         *cancellable,
                                             GError              **error);
  void           (*wait_async)              (IdeSubprocess        *self,
                                             GCancellable         *cancellable,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data);
  gboolean       (*wait_finish)             (IdeSubprocess        *self,
                                             GAsyncResult         *result,
                                             GError              **error);
  gboolean       (*get_successful)          (IdeSubprocess        *self);
  gboolean       (*get_if_exited)           (IdeSubprocess        *self);
  gint           (*get_exit_status)         (IdeSubprocess        *self);
  gboolean       (*get_if_signaled)         (IdeSubprocess        *self);
  gint           (*get_term_sig)            (IdeSubprocess        *self);
  gint           (*get_status)              (IdeSubprocess        *self);
  void           (*send_signal)             (IdeSubprocess        *self,
                                             gint                  signal_num);
  void           (*force_exit)              (IdeSubprocess        *self);
  gboolean       (*communicate)             (IdeSubprocess        *self,
                                             GBytes               *stdin_buf,
                                             GCancellable         *cancellable,
                                             GBytes              **stdout_buf,
                                             GBytes              **stderr_buf,
                                             GError              **error);
  gboolean       (*communicate_utf8)        (IdeSubprocess        *self,
                                             const gchar          *stdin_buf,
                                             GCancellable         *cancellable,
                                             gchar               **stdout_buf,
                                             gchar               **stderr_buf,
                                             GError              **error);
  void           (*communicate_async)       (IdeSubprocess        *self,
                                             GBytes               *stdin_buf,
                                             GCancellable         *cancellable,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data);
  gboolean       (*communicate_finish)      (IdeSubprocess        *self,
                                             GAsyncResult         *result,
                                             GBytes              **stdout_buf,
                                             GBytes              **stderr_buf,
                                             GError              **error);
  void           (*communicate_utf8_async)  (IdeSubprocess        *self,
                                             const gchar          *stdin_buf,
                                             GCancellable         *cancellable,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data);
  gboolean       (*communicate_utf8_finish) (IdeSubprocess        *self,
                                             GAsyncResult         *result,
                                             gchar               **stdout_buf,
                                             gchar               **stderr_buf,
                                             GError              **error);
};

const gchar   *ide_subprocess_get_identifier     (IdeSubprocess *self);
GInputStream  *ide_subprocess_get_stdout_pipe    (IdeSubprocess *self);
GInputStream  *ide_subprocess_get_stderr_pipe    (IdeSubprocess *self);
GOutputStream *ide_subprocess_get_stdin_pipe     (IdeSubprocess *self);
gboolean       ide_subprocess_wait               (IdeSubprocess        *self,
                                                  GCancellable         *cancellable,
                                                  GError              **error);
gboolean       ide_subprocess_wait_check         (IdeSubprocess        *self,
                                                  GCancellable         *cancellable,
                                                  GError              **error);
void           ide_subprocess_wait_async         (IdeSubprocess        *self,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
gboolean       ide_subprocess_wait_finish        (IdeSubprocess        *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
void           ide_subprocess_wait_check_async   (IdeSubprocess        *self,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
gboolean       ide_subprocess_wait_check_finish  (IdeSubprocess        *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
gboolean       ide_subprocess_check_exit_status  (IdeSubprocess        *self,
                                                  GError              **error);
gboolean       ide_subprocess_get_successful     (IdeSubprocess        *self);
gboolean       ide_subprocess_get_if_exited      (IdeSubprocess        *self);
gint           ide_subprocess_get_exit_status    (IdeSubprocess        *self);
gboolean       ide_subprocess_get_if_signaled    (IdeSubprocess        *self);
gint           ide_subprocess_get_term_sig       (IdeSubprocess        *self);
gint           ide_subprocess_get_status         (IdeSubprocess        *self);
void           ide_subprocess_send_signal        (IdeSubprocess        *self,
                                                  gint                  signal_num);
void           ide_subprocess_force_exit         (IdeSubprocess        *self);
gboolean       ide_subprocess_communicate        (IdeSubprocess        *self,
                                                  GBytes               *stdin_buf,
                                                  GCancellable         *cancellable,
                                                  GBytes              **stdout_buf,
                                                  GBytes              **stderr_buf,
                                                  GError              **error);
gboolean       ide_subprocess_communicate_utf8   (IdeSubprocess        *self,
                                                  const gchar          *stdin_buf,
                                                  GCancellable         *cancellable,
                                                  gchar               **stdout_buf,
                                                  gchar               **stderr_buf,
                                                  GError              **error);
void           ide_subprocess_communicate_async  (IdeSubprocess        *self,
                                                  GBytes               *stdin_buf,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
gboolean       ide_subprocess_communicate_finish (IdeSubprocess        *self,
                                                  GAsyncResult         *result,
                                                  GBytes              **stdout_buf,
                                                  GBytes              **stderr_buf,
                                                  GError              **error);
void           ide_subprocess_communicate_utf8_async  (IdeSubprocess        *self,
                                                       const gchar          *stdin_buf,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
gboolean       ide_subprocess_communicate_utf8_finish (IdeSubprocess        *self,
                                                       GAsyncResult         *result,
                                                       gchar               **stdout_buf,
                                                       gchar               **stderr_buf,
                                                       GError              **error);

G_END_DECLS

#endif /* IDE_SUBPROCESS_H */
