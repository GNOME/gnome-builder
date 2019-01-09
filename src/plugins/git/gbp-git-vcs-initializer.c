/* gbp-git-vcs-initializer.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-git-vcs-initializer"

#include "config.h"

#include <libgit2-glib/ggit.h>
#include <libide-threading.h>

#include "gbp-git-vcs-initializer.h"

struct _GbpGitVcsInitializer
{
  GObject parent_instance;
};

static void vcs_initializer_init (IdeVcsInitializerInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpGitVcsInitializer, gbp_git_vcs_initializer, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_INITIALIZER, vcs_initializer_init))

static void
gbp_git_vcs_initializer_class_init (GbpGitVcsInitializerClass *klass)
{
}

static void
gbp_git_vcs_initializer_init (GbpGitVcsInitializer *self)
{
}

static void
gbp_git_vcs_initializer_initialize_worker (IdeTask      *task,
                                           gpointer      source_object,
                                           gpointer      task_data,
                                           GCancellable *cancellable)
{
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GError) error = NULL;
  GFile *file = task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_GIT_VCS_INITIALIZER (source_object));
  g_assert (G_IS_FILE (file));

  repository = ggit_repository_init_repository (file, FALSE, &error);

  if (repository == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_git_vcs_initializer_initialize_async (IdeVcsInitializer   *initializer,
                                          GFile               *file,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GbpGitVcsInitializer *self = (GbpGitVcsInitializer *)initializer;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (GBP_IS_GIT_VCS_INITIALIZER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);
  ide_task_run_in_thread (task, gbp_git_vcs_initializer_initialize_worker);
}

static gboolean
gbp_git_vcs_initializer_initialize_finish (IdeVcsInitializer  *initializer,
                                           GAsyncResult       *result,
                                           GError            **error)
{
  g_return_val_if_fail (GBP_IS_GIT_VCS_INITIALIZER (initializer), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static gchar *
gbp_git_vcs_initializer_get_title (IdeVcsInitializer *initilizer)
{
  return g_strdup ("Git");
}

static void
vcs_initializer_init (IdeVcsInitializerInterface *iface)
{
  iface->get_title = gbp_git_vcs_initializer_get_title;
  iface->initialize_async = gbp_git_vcs_initializer_initialize_async;
  iface->initialize_finish = gbp_git_vcs_initializer_initialize_finish;
}
