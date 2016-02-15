/* ide-subprocess-launcher.h
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

#ifndef IDE_SUBPROCESS_LAUNCHER_H
#define IDE_SUBPROCESS_LAUNCHER_H

#include <gio/gio.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SUBPROCESS_LAUNCHER (ide_subprocess_launcher_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeSubprocessLauncher, ide_subprocess_launcher, IDE, SUBPROCESS_LAUNCHER, GObject)

struct _IdeSubprocessLauncherClass
{
  GObjectClass parent_class;

  GSubprocess *(*spawn_sync)   (IdeSubprocessLauncher  *self,
                                GCancellable           *cancellable,
                                GError                **error);
  void         (*spawn_async)  (IdeSubprocessLauncher  *self,
                                GCancellable           *cancellable,
                                GAsyncReadyCallback     callback,
                                gpointer                user_data);
  GSubprocess *(*spawn_finish) (IdeSubprocessLauncher  *self,
                                GAsyncResult           *result,
                                GError                **error);
};

IdeSubprocessLauncher *ide_subprocess_launcher_new                 (GSubprocessFlags       flags);
const gchar           *ide_subprocess_launcher_get_cwd             (IdeSubprocessLauncher *self);
void                   ide_subprocess_launcher_set_cwd             (IdeSubprocessLauncher *self,
                                                                    const gchar           *cwd);
GSubprocessFlags       ide_subprocess_launcher_get_flags           (IdeSubprocessLauncher *self);
void                   ide_subprocess_launcher_set_flags           (IdeSubprocessLauncher *self,
                                                                    GSubprocessFlags       flags);
const gchar * const   *ide_subprocess_launcher_get_environ         (IdeSubprocessLauncher *self);
void                   ide_subprocess_launcher_set_environ         (IdeSubprocessLauncher *self,
                                                                    const gchar * const   *environ_);
void                   ide_subprocess_launcher_setenv              (IdeSubprocessLauncher *self,
                                                                    const gchar           *key,
                                                                    const gchar           *value,
                                                                    gboolean               replace);
void                   ide_subprocess_launcher_overlay_environment (IdeSubprocessLauncher *self,
                                                                    IdeEnvironment        *environment);
void                   ide_subprocess_launcher_push_args           (IdeSubprocessLauncher *self,
                                                                    const gchar * const   *args);
void                   ide_subprocess_launcher_push_argv           (IdeSubprocessLauncher *self,
                                                                    const gchar           *argv);
GSubprocess           *ide_subprocess_launcher_spawn_sync          (IdeSubprocessLauncher  *self,
                                                                    GCancellable           *cancellable,
                                                                    GError                **error);
void                   ide_subprocess_launcher_spawn_async         (IdeSubprocessLauncher *self,
                                                                    GCancellable          *cancellable,
                                                                    GAsyncReadyCallback    callback,
                                                                    gpointer               user_data);
GSubprocess           *ide_subprocess_launcher_spawn_finish        (IdeSubprocessLauncher  *self,
                                                                    GAsyncResult           *result,
                                                                    GError                **error);

G_END_DECLS

#endif /* IDE_SUBPROCESS_LAUNCHER_H */
