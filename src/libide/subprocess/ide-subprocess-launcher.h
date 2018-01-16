/* ide-subprocess-launcher.h
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

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SUBPROCESS_LAUNCHER (ide_subprocess_launcher_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeSubprocessLauncher, ide_subprocess_launcher, IDE, SUBPROCESS_LAUNCHER, GObject)

struct _IdeSubprocessLauncherClass
{
  GObjectClass parent_class;

  IdeSubprocess *(*spawn) (IdeSubprocessLauncher  *self,
                           GCancellable           *cancellable,
                           GError                **error);

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IDE_AVAILABLE_IN_ALL
IdeSubprocessLauncher *ide_subprocess_launcher_new                 (GSubprocessFlags        flags);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_subprocess_launcher_get_cwd             (IdeSubprocessLauncher  *self);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_set_cwd             (IdeSubprocessLauncher  *self,
                                                                    const gchar            *cwd);
IDE_AVAILABLE_IN_ALL
GSubprocessFlags       ide_subprocess_launcher_get_flags           (IdeSubprocessLauncher  *self);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_set_flags           (IdeSubprocessLauncher  *self,
                                                                    GSubprocessFlags        flags);
IDE_AVAILABLE_IN_ALL
gboolean               ide_subprocess_launcher_get_run_on_host     (IdeSubprocessLauncher  *self);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_set_run_on_host     (IdeSubprocessLauncher  *self,
                                                                    gboolean                run_on_host);
IDE_AVAILABLE_IN_3_28
void                   ide_subprocess_launcher_append_path         (IdeSubprocessLauncher  *self,
                                                                    const gchar            *append_path);
IDE_AVAILABLE_IN_ALL
gboolean               ide_subprocess_launcher_get_clear_env       (IdeSubprocessLauncher  *self);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_set_clear_env       (IdeSubprocessLauncher  *self,
                                                                    gboolean                clear_env);
IDE_AVAILABLE_IN_ALL
const gchar * const   *ide_subprocess_launcher_get_environ         (IdeSubprocessLauncher  *self);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_set_environ         (IdeSubprocessLauncher  *self,
                                                                    const gchar * const    *environ_);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_subprocess_launcher_getenv              (IdeSubprocessLauncher  *self,
                                                                    const gchar            *key);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_setenv              (IdeSubprocessLauncher  *self,
                                                                    const gchar            *key,
                                                                    const gchar            *value,
                                                                    gboolean                replace);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_insert_argv         (IdeSubprocessLauncher  *self,
                                                                    guint                   index,
                                                                    const gchar            *arg);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_replace_argv        (IdeSubprocessLauncher  *self,
                                                                    guint                   index,
                                                                    const gchar            *arg);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_overlay_environment (IdeSubprocessLauncher  *self,
                                                                    IdeEnvironment         *environment);
IDE_AVAILABLE_IN_ALL
const gchar * const   *ide_subprocess_launcher_get_argv            (IdeSubprocessLauncher  *self);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_push_args           (IdeSubprocessLauncher  *self,
                                                                    const gchar * const    *args);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_push_argv           (IdeSubprocessLauncher  *self,
                                                                    const gchar            *argv);
IDE_AVAILABLE_IN_ALL
gchar                 *ide_subprocess_launcher_pop_argv            (IdeSubprocessLauncher  *self) G_GNUC_WARN_UNUSED_RESULT;
IDE_AVAILABLE_IN_ALL
IdeSubprocess         *ide_subprocess_launcher_spawn               (IdeSubprocessLauncher  *self,
                                                                    GCancellable           *cancellable,
                                                                    GError                **error);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_set_stdout_file_path(IdeSubprocessLauncher  *self,
                                                                    const gchar            *stdout_file_path);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_take_fd             (IdeSubprocessLauncher  *self,
                                                                    gint                    source_fd,
                                                                    gint                    dest_fd);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_take_stdin_fd       (IdeSubprocessLauncher  *self,
                                                                    gint                    stdin_fd);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_take_stdout_fd      (IdeSubprocessLauncher  *self,
                                                                    gint                    stdout_fd);
IDE_AVAILABLE_IN_ALL
void                   ide_subprocess_launcher_take_stderr_fd      (IdeSubprocessLauncher  *self,
                                                                    gint                    stderr_fd);

G_END_DECLS
