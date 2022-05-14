/* gbp-git-vcs-config.c
 *
 * Copyright 2016 Akshaya Kakkilaya <akshaya.kakkilaya@gmail.com>
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-git-vcs-config"

#include "config.h"

#include <libide-threading.h>
#include <libide-vcs.h>

#include "daemon/ipc-git-config.h"

#include "gbp-git-client.h"
#include "gbp-git-vcs.h"
#include "gbp-git-vcs-config.h"

struct _GbpGitVcsConfig
{
  IdeObject parent_instance;
  guint is_global : 1;
};

static void vcs_config_init (IdeVcsConfigInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitVcsConfig, gbp_git_vcs_config, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_CONFIG, vcs_config_init))

static const gchar *
get_key (IdeVcsConfigType type)
{
  switch (type)
    {
    case IDE_VCS_CONFIG_FULL_NAME:
      return "user.name";
    case IDE_VCS_CONFIG_EMAIL:
      return "user.email";
    default:
      return NULL;
    }
}

static IpcGitConfig *
get_config (GbpGitVcsConfig  *self,
            GCancellable     *cancellable,
            GError          **error)
{
  g_autofree gchar *obj_path = NULL;
  g_autoptr(IpcGitService) service = NULL;
  GDBusConnection *connection;
  GbpGitClient *client;
  IdeContext *context;

  g_assert (GBP_IS_GIT_VCS_CONFIG (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  client = gbp_git_client_from_context (context);

  g_assert (IDE_IS_CONTEXT (context));
  g_assert (GBP_IS_GIT_CLIENT (client));

  if (!(service = gbp_git_client_get_service (client, cancellable, error)))
    return NULL;

  if (!self->is_global)
    {
      IdeVcs *vcs;
      IpcGitRepository *repository;

      vcs = ide_vcs_from_context (context);
      g_assert (GBP_IS_GIT_VCS (vcs));

      repository = gbp_git_vcs_get_repository (GBP_GIT_VCS (vcs));
      g_assert (!repository || IPC_IS_GIT_REPOSITORY (repository));

      if (repository == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       "Failed to load git repository");
          return NULL;
        }

      if (!ipc_git_repository_call_load_config_sync (repository, &obj_path, cancellable, error))
        return NULL;
    }
  else
    {
      if (!ipc_git_service_call_load_config_sync (service, &obj_path, cancellable, error))
        return NULL;
    }

  g_assert (obj_path != NULL);
  g_assert (g_variant_is_object_path (obj_path));

  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (service));
  g_assert (G_IS_DBUS_CONNECTION (connection));

  return ipc_git_config_proxy_new_sync (connection,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        NULL,
                                        obj_path,
                                        cancellable,
                                        error);
}

static void
gbp_git_vcs_config_get_config (IdeVcsConfig    *config,
                               IdeVcsConfigType type,
                               GValue          *value)
{
  GbpGitVcsConfig *self = (GbpGitVcsConfig *)config;
  g_autoptr(IpcGitConfig) proxy = NULL;

  g_assert (GBP_IS_GIT_VCS_CONFIG (self));
  g_assert (value != NULL);

  if ((proxy = get_config (self, NULL, NULL)))
    {
      g_autofree gchar *str = NULL;

      ipc_git_config_call_read_key_sync (proxy, get_key (type), &str, NULL, NULL);
      ipc_git_config_call_close (proxy, NULL, NULL, NULL);

      g_value_set_string (value, str);
    }
}

static void
gbp_git_vcs_config_set_config (IdeVcsConfig     *config,
                               IdeVcsConfigType  type,
                               const GValue     *value)
{
  GbpGitVcsConfig *self = (GbpGitVcsConfig *)config;
  g_autoptr(IpcGitConfig) proxy = NULL;

  g_assert (GBP_IS_GIT_VCS_CONFIG (self));
  g_assert (value != NULL);

  if ((proxy = get_config (self, NULL, NULL)))
    {
      g_auto(GValue) str = G_VALUE_INIT;

      if (!G_VALUE_HOLDS_STRING (value))
        {
          g_value_init (&str, G_TYPE_STRING);
          g_value_transform (value, &str);
          value = &str;
        }

      ipc_git_config_call_write_key_sync (proxy,
                                          get_key (type),
                                          g_value_get_string (value),
                                          NULL, NULL);
      ipc_git_config_call_close (proxy, NULL, NULL, NULL);
    }
}

static void
vcs_config_init (IdeVcsConfigInterface *iface)
{
  iface->get_config = gbp_git_vcs_config_get_config;
  iface->set_config = gbp_git_vcs_config_set_config;
}

static void
gbp_git_vcs_config_class_init (GbpGitVcsConfigClass *klass)
{
}

static void
gbp_git_vcs_config_init (GbpGitVcsConfig *self)
{
  self->is_global = TRUE;
}

void
gbp_git_vcs_config_set_global (GbpGitVcsConfig *self,
                               gboolean         is_global)
{
  g_return_if_fail (GBP_IS_GIT_VCS_CONFIG (self));

  self->is_global = !!is_global;
}
