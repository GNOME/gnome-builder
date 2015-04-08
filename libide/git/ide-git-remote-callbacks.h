/* ide-git-remote-callbacks.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_GIT_REMOTE_CALLBACKS_H
#define IDE_GIT_REMOTE_CALLBACKS_H

#include <libgit2-glib/ggit.h>

#include "ide-progress.h"

G_BEGIN_DECLS

#define IDE_TYPE_GIT_REMOTE_CALLBACKS            (ide_git_remote_callbacks_get_type())
#define IDE_GIT_REMOTE_CALLBACKS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_GIT_REMOTE_CALLBACKS, IdeGitRemoteCallbacks))
#define IDE_GIT_REMOTE_CALLBACKS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_GIT_REMOTE_CALLBACKS, IdeGitRemoteCallbacks const))
#define IDE_GIT_REMOTE_CALLBACKS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_GIT_REMOTE_CALLBACKS, IdeGitRemoteCallbacksClass))
#define IDE_IS_GIT_REMOTE_CALLBACKS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_GIT_REMOTE_CALLBACKS))
#define IDE_IS_GIT_REMOTE_CALLBACKS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_GIT_REMOTE_CALLBACKS))
#define IDE_GIT_REMOTE_CALLBACKS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_GIT_REMOTE_CALLBACKS, IdeGitRemoteCallbacksClass))

typedef struct _IdeGitRemoteCallbacks      IdeGitRemoteCallbacks;
typedef struct _IdeGitRemoteCallbacksClass IdeGitRemoteCallbacksClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGitRemoteCallbacks, g_object_unref)

GType                ide_git_remote_callbacks_get_type     (void);
GgitRemoteCallbacks *ide_git_remote_callbacks_new          (void);
gdouble              ide_git_remote_callbacks_get_fraction (IdeGitRemoteCallbacks *self);
IdeProgress         *ide_git_remote_callbacks_get_progress (IdeGitRemoteCallbacks *self);

G_END_DECLS

#endif /* IDE_GIT_REMOTE_CALLBACKS_H */
