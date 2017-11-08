/* ide-runner.h
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include "ide-object.h"
#include "buildsystem/ide-environment.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUNNER (ide_runner_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeRunner, ide_runner, IDE, RUNNER, IdeObject)

struct _IdeRunnerClass
{
  IdeObjectClass parent;

  void                   (*force_quit)      (IdeRunner             *self);
  GOutputStream         *(*get_stdin)       (IdeRunner             *self);
  GInputStream          *(*get_stdout)      (IdeRunner             *self);
  GInputStream          *(*get_stderr)      (IdeRunner             *self);
  void                   (*run_async)       (IdeRunner             *self,
                                             GCancellable          *cancellable,
                                             GAsyncReadyCallback    callback,
                                             gpointer               user_data);
  gboolean               (*run_finish)      (IdeRunner             *self,
                                             GAsyncResult          *result,
                                             GError               **error);
  void                   (*set_tty)         (IdeRunner             *self,
                                             int                    tty_fd);
  IdeSubprocessLauncher *(*create_launcher) (IdeRunner             *self);
  void                   (*fixup_launcher)  (IdeRunner             *self,
                                             IdeSubprocessLauncher *launcher);
  IdeRuntime            *(*get_runtime)     (IdeRunner             *self);

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
};

IDE_AVAILABLE_IN_ALL
IdeRunner         *ide_runner_new             (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
gboolean           ide_runner_get_failed      (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
void               ide_runner_set_failed      (IdeRunner            *self,
                                               gboolean              failed);
IDE_AVAILABLE_IN_ALL
IdeRuntime        *ide_runner_get_runtime     (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
void               ide_runner_force_quit      (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
IdeEnvironment    *ide_runner_get_environment (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
void               ide_runner_run_async       (IdeRunner            *self,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_runner_run_finish      (IdeRunner            *self,
                                               GAsyncResult         *result,
                                               GError              **error);
IDE_AVAILABLE_IN_ALL
GSubprocessFlags   ide_runner_get_flags       (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
void               ide_runner_set_flags       (IdeRunner            *self,
                                               GSubprocessFlags      flags);
IDE_AVAILABLE_IN_ALL
const gchar       *ide_runner_get_cwd         (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
void               ide_runner_set_cwd         (IdeRunner            *self,
                                               const gchar          *cwd);
IDE_AVAILABLE_IN_ALL
gboolean           ide_runner_get_clear_env   (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
void               ide_runner_set_clear_env   (IdeRunner            *self,
                                               gboolean              clear_env);
IDE_AVAILABLE_IN_ALL
void               ide_runner_prepend_argv    (IdeRunner            *self,
                                               const gchar          *param);
IDE_AVAILABLE_IN_ALL
void               ide_runner_append_argv     (IdeRunner            *self,
                                               const gchar          *param);
IDE_AVAILABLE_IN_ALL
void               ide_runner_push_args       (IdeRunner            *self,
                                               const gchar * const  *args);
IDE_AVAILABLE_IN_ALL
gchar            **ide_runner_get_argv        (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
void               ide_runner_set_argv        (IdeRunner            *self,
                                               const gchar * const  *argv);
IDE_AVAILABLE_IN_ALL
gint               ide_runner_take_fd         (IdeRunner            *self,
                                               gint                  source_fd,
                                               gint                  dest_fd);
IDE_AVAILABLE_IN_ALL
GOutputStream     *ide_runner_get_stdin       (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
GInputStream      *ide_runner_get_stdout      (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
GInputStream      *ide_runner_get_stderr      (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
gboolean           ide_runner_get_run_on_host (IdeRunner            *self);
IDE_AVAILABLE_IN_ALL
void               ide_runner_set_run_on_host (IdeRunner            *self,
                                               gboolean              run_on_host);
IDE_AVAILABLE_IN_ALL
void               ide_runner_set_tty         (IdeRunner            *self,
                                               int                   tty_fd);
IDE_AVAILABLE_IN_ALL
gint               ide_runner_steal_tty       (IdeRunner            *self);

G_END_DECLS
