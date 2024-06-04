/* gbp-gitlab-workbench-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-gitlab-workbench-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <gitlab-glib.h>

#include <libide-gui.h>
#include <libide-threading.h>

#include "gbp-gitlab-workbench-addin.h"

#include "../git/gbp-git-vcs.h"

struct _GbpGitlabWorkbenchAddin
{
  GObject       parent_instance;

  IdeWorkbench *workbench;
  GitlabClient *client;
  IdeSettings  *settings;

  guint         project_loaded : 1;
};

enum {
  PROP_0,
  PROP_ENABLED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gbp_gitlab_workbench_addin_load (IdeWorkbenchAddin *addin,
                                 IdeWorkbench      *workbench)
{
  GbpGitlabWorkbenchAddin *self = (GbpGitlabWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GITLAB_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static gboolean
private_access_token_to_api_key (GValue   *value,
                                 GVariant *variant,
                                 gpointer  user_data)
{
  const char *str = g_variant_get_string (variant, NULL);

  if (!ide_str_empty0 (str))
    g_value_take_object (value, gitlab_api_key_new (str));

  return TRUE;
}

static void
changed_host_cb (GbpGitlabWorkbenchAddin *self,
                 const char              *key,
                 IdeSettings             *settings)
{
  g_assert (GBP_IS_GITLAB_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_SETTINGS (settings));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLED]);
}

static void
gbp_gitlab_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                           IdeProjectInfo    *project_info)
{
  GbpGitlabWorkbenchAddin *self = (GbpGitlabWorkbenchAddin *)addin;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GITLAB_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  context = ide_workbench_get_context (self->workbench);

  self->project_loaded = TRUE;
  self->client = gitlab_client_new (NULL, NULL);
  self->settings = ide_context_ref_settings (context, "org.gnome.builder.gitlab");

  ide_settings_bind (self->settings, "host",
                     self->client, "host",
                     G_SETTINGS_BIND_GET);
  ide_settings_bind_with_mapping (self->settings, "host",
                                  self->client, "secret",
                                  G_SETTINGS_BIND_GET,
                                  private_access_token_to_api_key,
                                  NULL, NULL, NULL);

  g_signal_connect_object (self->settings,
                           "changed::host",
                           G_CALLBACK (changed_host_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLED]);
}

static void
gbp_gitlab_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  GbpGitlabWorkbenchAddin *self = (GbpGitlabWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GITLAB_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_clear_object (&self->client);
  g_clear_object (&self->settings);

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_gitlab_workbench_addin_load;
  iface->unload = gbp_gitlab_workbench_addin_unload;
  iface->project_loaded = gbp_gitlab_workbench_addin_project_loaded;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitlabWorkbenchAddin, gbp_gitlab_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_gitlab_workbench_addin_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GbpGitlabWorkbenchAddin *self = GBP_GITLAB_WORKBENCH_ADDIN (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, gbp_gitlab_workbench_addin_get_enabled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gitlab_workbench_addin_class_init (GbpGitlabWorkbenchAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_gitlab_workbench_addin_get_property;

  properties[PROP_ENABLED] =
    g_param_spec_boolean ("enabled", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_gitlab_workbench_addin_init (GbpGitlabWorkbenchAddin *self)
{
}

gboolean
gbp_gitlab_workbench_addin_get_enabled (GbpGitlabWorkbenchAddin *self)
{
  g_autofree char *project_url = NULL;

  g_return_val_if_fail (GBP_IS_GITLAB_WORKBENCH_ADDIN (self), FALSE);

  project_url = gbp_gitlab_workbench_addin_get_project_url (self);

  return !ide_str_empty0 (project_url);
}

char *
gbp_gitlab_workbench_addin_get_project_url (GbpGitlabWorkbenchAddin *self)
{
  g_autofree char *host = NULL;
  g_autofree char *url = NULL;
  g_autofree char *path = NULL;
  g_autoptr(IdeVcsUri) uri = NULL;
  const char *skipped_path;
  IdeVcs *vcs;

  g_return_val_if_fail (GBP_IS_GITLAB_WORKBENCH_ADDIN (self), NULL);
  g_return_val_if_fail (IDE_IS_WORKBENCH (self->workbench), NULL);

  if (self->settings == NULL)
    return NULL;

  if (!(vcs = ide_workbench_get_vcs (self->workbench)))
    return NULL;

  if (!GBP_IS_GIT_VCS (vcs))
    return NULL;

  if (!(url = gbp_git_vcs_get_remote_url (GBP_GIT_VCS (vcs), "origin", NULL, NULL)))
    return NULL;

  if (!(uri = ide_vcs_uri_new (url)))
    return NULL;

  host = ide_settings_get_string (self->settings, "host");
  if (ide_str_empty0 (host))
    {
      const char *uri_host = ide_vcs_uri_get_host (uri);

      if (strstr (uri_host, "gitlab") == NULL)
        return NULL;

      host = g_strdup (uri_host);
    }

  path = g_strdup (ide_vcs_uri_get_path (uri));
  if (ide_str_empty0 (path))
    return NULL;

  if (g_str_has_suffix (path, ".git"))
    path[strlen(path)-4] = 0;

  if (g_str_has_prefix (path, "~"))
    skipped_path = path + 1;
  else
    skipped_path = path;

  if (skipped_path[0] != '/')
    {
      g_autofree char *tmp = path;
      path = g_strdup_printf ("/%s", skipped_path);
      skipped_path = path;
    }

  return g_uri_join (G_URI_FLAGS_NONE,
                     "https",
                     NULL,
                     host,
                     -1,
                     skipped_path,
                     NULL,
                     NULL);
}
