/* ipc-git-config-impl.c
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

#define G_LOG_DOMAIN "ipc-git-config-impl"

#include "ipc-git-config-impl.h"
#include "ipc-git-util.h"

struct _IpcGitConfigImpl
{
  IpcGitConfigSkeleton parent;
  GgitConfig *config;
};

static gboolean
ipc_git_config_impl_handle_read_key (IpcGitConfig          *config,
                                     GDBusMethodInvocation *invocation,
                                     const gchar           *key)
{
  IpcGitConfigImpl *self = (IpcGitConfigImpl *)config;
  g_autoptr(GgitConfig) snapshot = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *value;

  g_assert (IPC_IS_GIT_CONFIG (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (key != NULL);

  if (!(snapshot = ggit_config_snapshot (self->config, &error)) ||
      !(value = ggit_config_get_string (snapshot, key, &error)))
    g_dbus_method_invocation_return_dbus_error (invocation,
                                                "org.gnome.Builder.Git.Config.Error.NotFound",
                                                "No such key");
  else
    ipc_git_config_complete_read_key (config, invocation, value);

  return TRUE;
}

static gboolean
ipc_git_config_impl_handle_write_key (IpcGitConfig          *config,
                                      GDBusMethodInvocation *invocation,
                                      const gchar           *key,
                                      const gchar           *value)
{
  IpcGitConfigImpl *self = (IpcGitConfigImpl *)config;
  g_autoptr(GError) error = NULL;

  g_assert (IPC_IS_GIT_CONFIG (self));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));
  g_assert (key != NULL);

  if (!ggit_config_set_string (self->config, key, value, &error))
    complete_wrapped_error (invocation, error);
  else
    ipc_git_config_complete_write_key (config, invocation);

  return TRUE;
}

static gboolean
ipc_git_config_impl_handle_close (IpcGitConfig          *config,
                                  GDBusMethodInvocation *invocation)
{
  g_assert (IPC_IS_GIT_CONFIG (config));
  g_assert (G_IS_DBUS_METHOD_INVOCATION (invocation));

  ipc_git_config_emit_closed (config);
  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (config));
  ipc_git_config_complete_close (config, invocation);

  return TRUE;
}

static void
git_config_iface_init (IpcGitConfigIface *iface)
{
  iface->handle_read_key = ipc_git_config_impl_handle_read_key;
  iface->handle_write_key = ipc_git_config_impl_handle_write_key;
  iface->handle_close = ipc_git_config_impl_handle_close;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IpcGitConfigImpl, ipc_git_config_impl, IPC_TYPE_GIT_CONFIG_SKELETON,
                         G_IMPLEMENT_INTERFACE (IPC_TYPE_GIT_CONFIG, git_config_iface_init))

static void
ipc_git_config_impl_finalize (GObject *object)
{
  IpcGitConfigImpl *self = (IpcGitConfigImpl *)object;

  g_clear_object (&self->config);

  G_OBJECT_CLASS (ipc_git_config_impl_parent_class)->finalize (object);
}

static void
ipc_git_config_impl_class_init (IpcGitConfigImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ipc_git_config_impl_finalize;
}

static void
ipc_git_config_impl_init (IpcGitConfigImpl *self)
{
}

IpcGitConfig *
ipc_git_config_impl_new (GgitConfig *config)
{
  IpcGitConfigImpl *ret;

  g_return_val_if_fail (GGIT_IS_CONFIG (config), NULL);

  ret = g_object_new (IPC_TYPE_GIT_CONFIG_IMPL, NULL);
  ret->config = g_object_ref (config);

  return IPC_GIT_CONFIG (g_steal_pointer (&ret));
}
