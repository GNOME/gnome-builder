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

#include "gbp-git-client.h"
#include "gbp-git-vcs-initializer.h"

struct _GbpGitVcsInitializer
{
  IdeObject parent_instance;
};

static gchar *
gbp_git_vcs_initializer_get_title (IdeVcsInitializer *self)
{
  return g_strdup ("Git");
}

static void
create_cb (GObject      *object,
           GAsyncResult *result,
           gpointer      user_data)
{
  IpcGitService *service = (IpcGitService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *git_location = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IPC_IS_GIT_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ipc_git_service_call_create_finish (service, &git_location, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
get_service_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  GbpGitClient *client = (GbpGitClient *)object;
  g_autoptr(IpcGitService) service = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  cancellable = ide_task_get_cancellable (task);
  file = ide_task_get_task_data (task);

  if (!(service = gbp_git_client_get_service_finish (client, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ipc_git_service_call_create (service,
                                 g_file_peek_path (file),
                                 FALSE,
                                 cancellable,
                                 create_cb,
                                 g_steal_pointer (&task));
}

static void
gbp_git_vcs_initializer_initialize_async (IdeVcsInitializer   *initializer,
                                          GFile               *file,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GbpGitClient *client;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS_INITIALIZER (initializer));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (initializer, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_initializer_initialize_async);
  ide_task_set_task_data (task, g_file_dup (file), g_object_unref);

  context = ide_object_get_context (IDE_OBJECT (initializer));
  client = gbp_git_client_from_context (context);

  gbp_git_client_get_service_async (client,
                                    cancellable,
                                    get_service_cb,
                                    g_steal_pointer (&task));
}

static gboolean
gbp_git_vcs_initializer_initialize_finish (IdeVcsInitializer  *initializer,
                                           GAsyncResult       *result,
                                           GError            **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS_INITIALIZER (initializer));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
vcs_initializer_iface_init (IdeVcsInitializerInterface *iface)
{
  iface->get_title = gbp_git_vcs_initializer_get_title;
  iface->initialize_async = gbp_git_vcs_initializer_initialize_async;
  iface->initialize_finish = gbp_git_vcs_initializer_initialize_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitVcsInitializer, gbp_git_vcs_initializer, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_INITIALIZER, vcs_initializer_iface_init))

static void
gbp_git_vcs_initializer_class_init (GbpGitVcsInitializerClass *klass)
{
}

static void
gbp_git_vcs_initializer_init (GbpGitVcsInitializer *self)
{
}
