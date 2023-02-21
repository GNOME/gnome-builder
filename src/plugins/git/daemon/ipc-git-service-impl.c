/* ipc-git-service-impl.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define G_LOG_DOMAIN "ipc-git-service-impl"

#include <glib/gstdio.h>

#include <libgit2-glib/ggit.h>

#include "ipc-git-config-impl.h"
#include "ipc-git-progress.h"
#include "ipc-git-remote-callbacks.h"
#include "ipc-git-repository-impl.h"
#include "ipc-git-service-impl.h"
#include "ipc-git-types.h"
#include "ipc-git-util.h"

struct _IpcGitServiceImpl
{
  IpcGitServiceSkeleton  parent;
  GHashTable            *repos;
  GHashTable            *configs;
};

static gboolean
ipc_git_service_impl_handle_discover (IpcGitService         *service,
                                      GDBusMethodInvocation *invocation,
                                      const gchar           *location)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) found = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IPC_IS_GIT_SERVICE (service));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (location != NULL);

  file = g_file_new_for_path (location);
  found = ggit_repository_discover_full (file, TRUE, NULL, &error);

  if (error != NULL)
    complete_wrapped_error (invocation, error);
  else
    ipc_git_service_complete_discover (service, invocation, g_file_get_path (found));

  return TRUE;
}

static gboolean
ipc_git_service_impl_handle_create (IpcGitService         *service,
                                    GDBusMethodInvocation *invocation,
                                    const gchar           *location,
                                    gboolean               is_bare)
{
  IpcGitServiceImpl *self = (IpcGitServiceImpl *)service;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) found = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *path = NULL;

  g_assert (IPC_IS_GIT_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (location != NULL);

  file = g_file_new_for_path (location);

  if (!(repository = ggit_repository_init_repository (file, is_bare, &error)))
    return complete_wrapped_error (invocation, error);

  found = ggit_repository_get_location (repository);
  path = g_file_get_path (found);

  ipc_git_service_complete_create (service, invocation, path);

  return TRUE;
}

static void
ipc_git_service_impl_repository_closed_cb (IpcGitServiceImpl    *self,
                                           IpcGitRepositoryImpl *repository)
{
  g_assert (IPC_IS_GIT_SERVICE_IMPL (self));
  g_assert (IPC_IS_GIT_REPOSITORY_IMPL (repository));

  g_hash_table_remove (self->repos, repository);
}

static gboolean
ipc_git_service_impl_handle_open (IpcGitService         *service,
                                  GDBusMethodInvocation *invocation,
                                  const gchar           *location)
{
  IpcGitServiceImpl *self = (IpcGitServiceImpl *)service;
  g_autoptr(IpcGitRepository) repository = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) found = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *uuid = NULL;
  g_autofree gchar *obj_path = NULL;
  GDBusConnection *conn;

  g_assert (IPC_IS_GIT_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (location != NULL);

  file = g_file_new_for_path (location);

  if (!(repository = ipc_git_repository_impl_open (file, &error)))
    return complete_wrapped_error (invocation, error);

  conn = g_dbus_method_invocation_get_connection (invocation);
  uuid = g_dbus_generate_guid ();
  obj_path = g_strdup_printf ("/org/gnome/Builder/Repository/%s", uuid);
  g_hash_table_insert (self->repos, g_object_ref (repository), g_strdup (uuid));

  g_signal_connect_object (repository,
                           "closed",
                           G_CALLBACK (ipc_git_service_impl_repository_closed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  if (g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (repository), conn, obj_path, &error))
    ipc_git_service_complete_open (service, invocation, obj_path);
  else
    complete_wrapped_error (invocation, error);

  return TRUE;
}

typedef struct
{
  GDBusMethodInvocation *invocation;
  char                  *url;
  char                  *location;
  char                  *branch;
  GVariant              *config_options;
  char                  *progress_path;
  int                    clone_pty;
} Clone;

static void
clone_free (Clone *c)
{
  g_clear_pointer (&c->url, g_free);
  g_clear_pointer (&c->location, g_free);
  g_clear_pointer (&c->branch, g_free);
  g_clear_pointer (&c->progress_path, g_free);
  g_clear_pointer (&c->config_options, g_variant_unref);
  g_clear_object (&c->invocation);
  g_clear_fd (&c->clone_pty, NULL);
  g_slice_free (Clone, c);
}

static void
ipc_git_service_impl_clone_worker (GTask        *task,
                                   gpointer      source_object,
                                   gpointer      task_data,
                                   GCancellable *cancellable)
{
  Clone *c = task_data;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GgitCloneOptions) options = NULL;
  g_autoptr(GgitRemoteCallbacks) callbacks = NULL;
  g_autoptr(IpcGitProgress) progress = NULL;
  GgitFetchOptions *fetch_options = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) clone_location = NULL;
  GVariantIter iter;

  g_assert (G_IS_TASK (task));
  g_assert (IPC_IS_GIT_SERVICE_IMPL (source_object));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  progress = ipc_git_progress_proxy_new_sync (g_dbus_method_invocation_get_connection (c->invocation),
                                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                              NULL,
                                              c->progress_path,
                                              NULL,
                                              &error);
  if (progress == NULL)
    goto gerror;

  file = g_file_new_for_path (c->location);

  callbacks = ipc_git_remote_callbacks_new (progress, c->clone_pty);

  fetch_options = ggit_fetch_options_new ();
  ggit_fetch_options_set_remote_callbacks (fetch_options, callbacks);
  ggit_fetch_options_set_download_tags (fetch_options, FALSE);

  options = ggit_clone_options_new ();
  ggit_clone_options_set_checkout_branch (options, c->branch);
  ggit_clone_options_set_fetch_options (options, fetch_options);

  if (!(repository = ggit_repository_clone (c->url, file, options, &error)))
    goto gerror;

  if (g_variant_iter_init (&iter, c->config_options))
    {
      g_autoptr(GgitConfig) config = NULL;
      GVariant *value;
      gchar *key;

      if ((config = ggit_repository_get_config (repository, NULL)))
        {
          while (g_variant_iter_loop (&iter, "{sv}", &key, &value))
            {
              if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
                ggit_config_set_string (config, key, g_variant_get_string (value, NULL), NULL);
            }
        }
    }

  clone_location = ggit_repository_get_location (repository);
  ipc_git_service_complete_clone (source_object,
                                  g_steal_pointer (&c->invocation),
                                  NULL,
                                  g_file_peek_path (clone_location));

gerror:
  if (error != NULL)
    complete_wrapped_error (g_steal_pointer (&c->invocation), error);

  g_clear_pointer (&fetch_options, ggit_fetch_options_free);
}

static gboolean
ipc_git_service_impl_handle_clone (IpcGitService         *service,
                                   GDBusMethodInvocation *invocation,
                                   GUnixFDList           *fd_list,
                                   const gchar           *url,
                                   const gchar           *location,
                                   const gchar           *branch,
                                   GVariant              *config_options,
                                   const gchar           *progress_path,
                                   GVariant              *handle_variant)
{
  g_autoptr(GTask) task = NULL;
  int handle;
  Clone *c;

  g_assert (IPC_IS_GIT_SERVICE_IMPL (service));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (G_IS_UNIX_FD_LIST (fd_list));
  g_assert (url != NULL);
  g_assert (branch != NULL);
  g_assert (location != NULL);
  g_assert (g_variant_is_of_type (handle_variant, G_VARIANT_TYPE_HANDLE));

  handle = g_variant_get_handle (handle_variant);

  c = g_slice_new0 (Clone);
  c->url = g_strdup (url);
  c->location = g_strdup (location);
  c->branch = branch[0] ? g_strdup (branch) : NULL;
  c->config_options = g_variant_ref (config_options);
  c->progress_path = g_strdup (progress_path);
  c->invocation = g_steal_pointer (&invocation);
  c->clone_pty = g_unix_fd_list_get (fd_list, handle, NULL);

  task = g_task_new (service, NULL, NULL, NULL);
  g_task_set_source_tag (task, ipc_git_service_impl_handle_clone);
  g_task_set_task_data (task, c, (GDestroyNotify)clone_free);
  g_task_run_in_thread (task, ipc_git_service_impl_clone_worker);

  return TRUE;
}

static void
ipc_git_service_impl_config_closed_cb (IpcGitServiceImpl *self,
                                       IpcGitConfigImpl  *config)
{
  g_assert (IPC_IS_GIT_SERVICE_IMPL (self));
  g_assert (IPC_IS_GIT_CONFIG_IMPL (config));

  g_hash_table_remove (self->configs, config);
}

static gboolean
ipc_git_service_impl_handle_load_config (IpcGitService         *service,
                                         GDBusMethodInvocation *invocation)
{
  IpcGitServiceImpl *self = (IpcGitServiceImpl *)service;
  g_autoptr(IpcGitConfig) config = NULL;
  g_autoptr(GgitConfig) gconfig = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *uuid = NULL;
  g_autofree gchar *obj_path = NULL;
  GDBusConnection *conn;

  g_assert (IPC_IS_GIT_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (!(gconfig = ggit_config_new_default (&error)))
    return complete_wrapped_error (invocation, error);

  config = ipc_git_config_impl_new (gconfig);

  conn = g_dbus_method_invocation_get_connection (invocation);
  uuid = g_dbus_generate_guid ();
  obj_path = g_strdup_printf ("/org/gnome/Builder/Config/%s", uuid);
  g_hash_table_insert (self->configs, g_object_ref (config), g_strdup (uuid));

  g_signal_connect_object (config,
                           "closed",
                           G_CALLBACK (ipc_git_service_impl_config_closed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  if (g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (config), conn, obj_path, &error))
    ipc_git_service_complete_load_config (service, invocation, obj_path);
  else
    complete_wrapped_error (invocation, error);

  return TRUE;
}

static void
rm_rf (const char *dir)
{
  g_spawn_sync (NULL,
                (char **)(const char * const[]) { "rm", "-rf", dir, NULL },
                NULL,
                G_SPAWN_SEARCH_PATH,
                NULL, NULL,
                NULL, NULL, NULL, NULL);
}

typedef GgitRemoteHead **RemoteHeadList;

static void
remote_head_list_free (RemoteHeadList list)
{
  for (guint i = 0; list[i]; i++)
    ggit_remote_head_unref (list[i]);
  g_free (list);
}

G_DEFINE_AUTO_CLEANUP_FREE_FUNC (RemoteHeadList, remote_head_list_free, NULL)

typedef struct
{
  GgitRemoteCallbacks *callbacks;
  char                *uri;
  IpcGitRefKind        kind;
} ListRemoteRefsByKind;

static void
list_remote_refs_by_kind_free (gpointer data)
{
  ListRemoteRefsByKind *state = data;

  g_clear_pointer (&state->uri, g_free);
  g_clear_object (&state->callbacks);
  g_slice_free (ListRemoteRefsByKind, state);
}

static void
list_remote_refs_by_kind_worker (GTask        *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  ListRemoteRefsByKind *state = task_data;
  g_autoptr(GgitRepository) repo = NULL;
  g_autoptr(GgitRemote) remote = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) repodir = NULL;
  g_autofree char *tmpdir = NULL;
  g_auto(RemoteHeadList) refs = NULL;
  GPtrArray *ar;

  g_assert (G_IS_TASK (task));
  g_assert (IPC_IS_GIT_SERVICE_IMPL (source_object));
  g_assert (task_data != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Only branches are currently supported, tags are ignored */

  if (!(tmpdir = g_dir_make_tmp (".libgit2-glib-remote-ls-XXXXXX", &error)))
    goto handle_gerror;

  repodir = g_file_new_for_path (tmpdir);

  if (!(repo = ggit_repository_init_repository (repodir, TRUE, &error)))
    goto handle_gerror;

  if (!(remote = ggit_remote_new_anonymous (repo, state->uri, &error)))
    goto handle_gerror;

  ggit_remote_connect (remote,
                       GGIT_DIRECTION_FETCH,
                       state->callbacks,
                       NULL, NULL, &error);
  if (error != NULL)
    goto handle_gerror;

  if (!(refs = ggit_remote_list (remote, &error)))
    goto handle_gerror;

  ar = g_ptr_array_new ();
  for (guint i = 0; refs[i]; i++)
    g_ptr_array_add (ar, g_strdup (ggit_remote_head_get_name (refs[i])));
  g_ptr_array_add (ar, NULL);

  g_task_return_pointer (task,
                         g_ptr_array_free (ar, FALSE),
                         (GDestroyNotify) g_strfreev);

  goto cleanup_tmpdir;

handle_gerror:
  g_task_return_error (task, g_steal_pointer (&error));

cleanup_tmpdir:
  if (tmpdir != NULL)
    rm_rf (tmpdir);
}

static void
list_remote_refs_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  IpcGitServiceImpl *self = (IpcGitServiceImpl *)object;
  g_autoptr(GDBusMethodInvocation) invocation = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) refs = NULL;

  g_assert (IPC_IS_GIT_SERVICE_IMPL (self));
  g_assert (G_IS_TASK (result));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (!(refs = g_task_propagate_pointer (G_TASK (result), &error)))
    complete_wrapped_error (g_steal_pointer (&invocation), error);
  else
    ipc_git_service_complete_list_remote_refs_by_kind (IPC_GIT_SERVICE (self),
                                                       g_steal_pointer (&invocation),
                                                       (const char * const *)refs);
}

static gboolean
ipc_git_service_impl_list_remote_refs_by_kind (IpcGitService         *service,
                                               GDBusMethodInvocation *invocation,
                                               const char            *uri,
                                               guint                  kind)
{
  IpcGitServiceImpl *self = (IpcGitServiceImpl *)service;
  g_autoptr(GTask) task = NULL;
  ListRemoteRefsByKind *state;

  g_assert (IPC_IS_GIT_SERVICE_IMPL (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  if (kind != IPC_GIT_REF_BRANCH)
    {
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.freedesktop.DBus.Error.InvalidArgs",
                                                  "kind must be a branch, tags are unsupported");
      return TRUE;
    }

  state = g_slice_new0 (ListRemoteRefsByKind);
  state->callbacks = ipc_git_remote_callbacks_new (NULL, -1);
  state->uri = g_strdup (uri);
  state->kind = kind;

  task = g_task_new (service, NULL, list_remote_refs_cb, g_steal_pointer (&invocation));
  g_task_set_task_data (task, state, list_remote_refs_by_kind_free);
  g_task_run_in_thread (task, list_remote_refs_by_kind_worker);

  return TRUE;
}

static void
git_service_iface_init (IpcGitServiceIface *iface)
{
  iface->handle_discover = ipc_git_service_impl_handle_discover;
  iface->handle_open = ipc_git_service_impl_handle_open;
  iface->handle_create = ipc_git_service_impl_handle_create;
  iface->handle_clone = ipc_git_service_impl_handle_clone;
  iface->handle_load_config = ipc_git_service_impl_handle_load_config;
  iface->handle_list_remote_refs_by_kind = ipc_git_service_impl_list_remote_refs_by_kind;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IpcGitServiceImpl, ipc_git_service_impl, IPC_TYPE_GIT_SERVICE_SKELETON,
                               G_IMPLEMENT_INTERFACE (IPC_TYPE_GIT_SERVICE, git_service_iface_init))

static void
ipc_git_service_impl_finalize (GObject *object)
{
  IpcGitServiceImpl *self = (IpcGitServiceImpl *)object;

  g_clear_pointer (&self->configs, g_hash_table_unref);
  g_clear_pointer (&self->repos, g_hash_table_unref);

  G_OBJECT_CLASS (ipc_git_service_impl_parent_class)->finalize (object);
}

static void
ipc_git_service_impl_class_init (IpcGitServiceImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ipc_git_service_impl_finalize;
}

static void
ipc_git_service_impl_init (IpcGitServiceImpl *self)
{
  self->configs = g_hash_table_new_full (NULL, NULL, g_object_unref, g_free);
  self->repos = g_hash_table_new_full (NULL, NULL, g_object_unref, g_free);
}

IpcGitService *
ipc_git_service_impl_new (void)
{
  return g_object_new (IPC_TYPE_GIT_SERVICE_IMPL, NULL);
}
