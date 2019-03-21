/* gbp-git-vcs-config.c
 *
 * Copyright 2016 Akshaya Kakkilaya <akshaya.kakkilaya@gmail.com>
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

#include <libide-vcs.h>

#include "gbp-git-client.h"
#include "gbp-git-vcs-config.h"

struct _GbpGitVcsConfig
{
  IdeObject parent_instance;
};

static void vcs_config_init (IdeVcsConfigInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpGitVcsConfig, gbp_git_vcs_config, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_CONFIG, vcs_config_init))

GbpGitVcsConfig *
gbp_git_vcs_config_new (void)
{
  return g_object_new (GBP_TYPE_GIT_VCS_CONFIG, NULL);
}

static void
gbp_git_vcs_config_get_string (GbpGitClient  *client,
                               const gchar   *key,
                               GValue        *value,
                               GError       **error)
{
  g_autoptr(GVariant) v = NULL;

  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (key != NULL);

  /*
   * Not ideal to communicate accross processes synchronously here,
   * but it's fine for now until we have async APIs for this (or implement
   * configuration reading in process with caching.
   */

  if ((v = gbp_git_client_read_config (client, key, NULL, error)))
    {
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, g_variant_get_string (v, NULL));
    }
}

static void
gbp_git_vcs_config_set_string (GbpGitClient  *client,
                               const gchar   *key,
                               const GValue  *value,
                               GError       **error)
{
  g_autoptr(GVariant) v = NULL;

  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (key != NULL);

  if (G_VALUE_HOLDS_STRING (value))
    v = g_variant_take_ref (g_variant_new_string (g_value_get_string (value)));

  gbp_git_client_update_config (client, TRUE, key, v, NULL, error);
}

static void
gbp_git_vcs_config_get_config (IdeVcsConfig    *self,
                               IdeVcsConfigType type,
                               GValue          *value)
{
  GbpGitClient *client;
  IdeContext *context;

  g_return_if_fail (GBP_IS_GIT_VCS_CONFIG (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  client = gbp_git_client_from_context (context);

  switch (type)
    {
    case IDE_VCS_CONFIG_FULL_NAME:
      gbp_git_vcs_config_get_string (client, "user.name", value, NULL);
      break;

    case IDE_VCS_CONFIG_EMAIL:
      gbp_git_vcs_config_get_string (client, "user.email", value, NULL);
      break;

    default:
      break;
    }
}

static void
gbp_git_vcs_config_set_config (IdeVcsConfig    *self,
                               IdeVcsConfigType type,
                               const GValue    *value)
{
  GbpGitClient *client;
  IdeContext *context;

  g_return_if_fail (GBP_IS_GIT_VCS_CONFIG (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  client = gbp_git_client_from_context (context);

  switch (type)
    {
    case IDE_VCS_CONFIG_FULL_NAME:
      gbp_git_vcs_config_set_string (client, "user.name", value, NULL);
      break;

    case IDE_VCS_CONFIG_EMAIL:
      gbp_git_vcs_config_set_string (client, "user.email", value, NULL);
      break;

    default:
      break;
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
}
