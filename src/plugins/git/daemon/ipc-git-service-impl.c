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

#include <libgit2-glib/ggit.h>

#include "ipc-git-config-impl.h"
#include "ipc-git-progress.h"
#include "ipc-git-remote-callbacks.h"
#include "ipc-git-repository-impl.h"
#include "ipc-git-service-impl.h"
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

static gboolean
ipc_git_service_impl_handle_clone (IpcGitService         *service,
                                   GDBusMethodInvocation *invocation,
                                   const gchar           *url,
                                   const gchar           *location,
                                   const gchar           *branch,
                                   GVariant              *config_options,
                                   const gchar           *progress_path)
{
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GgitCloneOptions) options = NULL;
  g_autoptr(GgitRemoteCallbacks) callbacks = NULL;
  g_autoptr(IpcGitProgress) progress = NULL;
  GgitFetchOptions *fetch_options = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) clone_location = NULL;
  GVariantIter iter;

  g_assert (IPC_IS_GIT_SERVICE_IMPL (service));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (url != NULL);
  g_assert (branch != NULL);
  g_assert (location != NULL);

  /* XXX: Make this threaded */

  progress = ipc_git_progress_proxy_new_sync (g_dbus_method_invocation_get_connection (invocation),
                                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                              NULL,
                                              progress_path,
                                              NULL,
                                              &error);
  if (progress == NULL)
    goto gerror;

  file = g_file_new_for_path (location);

  callbacks = ipc_git_remote_callbacks_new (progress);

  fetch_options = ggit_fetch_options_new ();
  ggit_fetch_options_set_remote_callbacks (fetch_options, callbacks);
  ggit_fetch_options_set_download_tags (fetch_options, FALSE);

  options = ggit_clone_options_new ();
  ggit_clone_options_set_checkout_branch (options, branch);
  ggit_clone_options_set_fetch_options (options, fetch_options);

  if (!(repository = ggit_repository_clone (url, file, options, &error)))
    goto gerror;

  if (g_variant_iter_init (&iter, config_options))
    {
      g_autoptr(GgitConfig) config = NULL;
      GVariant *value;
      gchar *key;

      if ((config = ggit_repository_get_config (repository, NULL)))
        {
          while (g_variant_iter_loop (&iter, "{sv}", &key, &value))
            {
              g_printerr ("%s\n", g_variant_get_type_string (value));
              if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
                ggit_config_set_string (config, key, g_variant_get_string (value, NULL), NULL);
            }
        }
    }

  clone_location = ggit_repository_get_location (repository);
  ipc_git_service_complete_clone (service,
                                  invocation,
                                  g_file_get_path (clone_location));

gerror:
  if (error != NULL)
    complete_wrapped_error (invocation, error);

  g_clear_pointer (&fetch_options, ggit_fetch_options_free);

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
git_service_iface_init (IpcGitServiceIface *iface)
{
  iface->handle_discover = ipc_git_service_impl_handle_discover;
  iface->handle_open = ipc_git_service_impl_handle_open;
  iface->handle_create = ipc_git_service_impl_handle_create;
  iface->handle_clone = ipc_git_service_impl_handle_clone;
  iface->handle_load_config = ipc_git_service_impl_handle_load_config;
}

G_DEFINE_TYPE_WITH_CODE (IpcGitServiceImpl, ipc_git_service_impl, IPC_TYPE_GIT_SERVICE_SKELETON,
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
