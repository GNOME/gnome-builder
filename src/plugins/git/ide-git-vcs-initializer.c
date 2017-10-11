/* ide-git-vcs-initializer.c
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

#include <libgit2-glib/ggit.h>

#include "ide-git-vcs-initializer.h"

struct _IdeGitVcsInitializer
{
  GObject parent_instance;
};

static void vcs_initializer_init (IdeVcsInitializerInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGitVcsInitializer, ide_git_vcs_initializer, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_INITIALIZER, vcs_initializer_init))

IdeGitVcsInitializer *
ide_git_vcs_initializer_new (void)
{
  return g_object_new (IDE_TYPE_GIT_VCS_INITIALIZER, NULL);
}

static void
ide_git_vcs_initializer_class_init (IdeGitVcsInitializerClass *klass)
{
}

static void
ide_git_vcs_initializer_init (IdeGitVcsInitializer *self)
{
}

static void
ide_git_vcs_initializer_initialize_worker (GTask        *task,
                                           gpointer      source_object,
                                           gpointer      task_data,
                                           GCancellable *cancellable)
{
  GFile *file = task_data;
  g_autoptr(GgitRepository) repository = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_GIT_VCS_INITIALIZER (source_object));
  g_assert (G_IS_FILE (file));

  repository = ggit_repository_init_repository (file, FALSE, &error);

  if (repository == NULL)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

static void
ide_git_vcs_initializer_initialize_async (IdeVcsInitializer   *initializer,
                                          GFile               *file,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  IdeGitVcsInitializer *self = (IdeGitVcsInitializer *)initializer;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_GIT_VCS_INITIALIZER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);
  g_task_run_in_thread (task, ide_git_vcs_initializer_initialize_worker);
}

static gboolean
ide_git_vcs_initializer_initialize_finish (IdeVcsInitializer  *initializer,
                                           GAsyncResult       *result,
                                           GError            **error)
{
  g_return_val_if_fail (IDE_IS_GIT_VCS_INITIALIZER (initializer), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gchar *
ide_git_vcs_initializer_get_title (IdeVcsInitializer *initilizer)
{
  return g_strdup ("Git");
}

static void
vcs_initializer_init (IdeVcsInitializerInterface *iface)
{
  iface->get_title = ide_git_vcs_initializer_get_title;
  iface->initialize_async = ide_git_vcs_initializer_initialize_async;
  iface->initialize_finish = ide_git_vcs_initializer_initialize_finish;
}
