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

#include "gbp-git-client.h"
#include "gbp-git-vcs-initializer.h"

struct _GbpGitVcsInitializer
{
  IdeObject parent_instance;
};

static void vcs_initializer_init (IdeVcsInitializerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpGitVcsInitializer, gbp_git_vcs_initializer, IDE_TYPE_OBJECT,
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
gbp_git_vcs_initializer_initialize_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GbpGitClient *client = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_git_client_create_repo_finish (client, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_git_vcs_initializer_initialize_async (IdeVcsInitializer   *initializer,
                                          GFile               *file,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GbpGitVcsInitializer *self = (GbpGitVcsInitializer *)initializer;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeTask) task = NULL;
  GbpGitClient *client;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_GIT_VCS_INITIALIZER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_initializer_initialize_async);

  context = ide_object_ref_context (IDE_OBJECT (self));
  client = gbp_git_client_from_context (context);

  gbp_git_client_create_repo_async (client,
                                    file,
                                    FALSE,
                                    cancellable,
                                    gbp_git_vcs_initializer_initialize_cb,
                                    g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_git_vcs_initializer_initialize_finish (IdeVcsInitializer  *initializer,
                                           GAsyncResult       *result,
                                           GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (GBP_IS_GIT_VCS_INITIALIZER (initializer), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
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
