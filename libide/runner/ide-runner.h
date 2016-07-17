/* ide-runner.h
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

#ifndef IDE_RUNNER_H
#define IDE_RUNNER_H

#include <gio/gio.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUNNER (ide_runner_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeRunner, ide_runner, IDE, RUNNER, IdeObject)

struct _IdeRunnerClass
{
  IdeObjectClass parent;

  void           (*force_quit) (IdeRunner            *self);
  GInputStream  *(*get_stdin)  (IdeRunner            *self);
  GOutputStream *(*get_stdout) (IdeRunner            *self);
  GOutputStream *(*get_stderr) (IdeRunner            *self);
  void           (*run_async)  (IdeRunner            *self,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data);
  gboolean       (*run_finish) (IdeRunner            *self,
                                GAsyncResult         *result,
                                GError              **error);
};

IdeRunner      *ide_runner_new          (IdeContext           *context);
void            ide_runner_force_quit   (IdeRunner            *self);
void            ide_runner_run_async    (IdeRunner            *self,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data);
gboolean        ide_runner_run_finish   (IdeRunner            *self,
                                         GAsyncResult         *result,
                                         GError              **error);
void            ide_runner_prepend_argv (IdeRunner            *self,
                                         const gchar          *param);
void            ide_runner_append_argv  (IdeRunner            *self,
                                         const gchar          *param);
gchar         **ide_runner_get_argv     (IdeRunner            *self);
void            ide_runner_set_argv     (IdeRunner            *self,
                                         const gchar * const  *argv);
GInputStream   *ide_runner_get_stdin    (IdeRunner            *self);
GOutputStream  *ide_runner_get_stdout   (IdeRunner            *self);
GOutputStream  *ide_runner_get_stderr   (IdeRunner            *self);

G_END_DECLS

#endif /* IDE_RUNNER_H */
