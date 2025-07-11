/* gbp-git-tweaks-addin.c
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

#define G_LOG_DOMAIN "gbp-git-tweaks-addin"

#include "config.h"

#include <libide-vcs.h>

#include "gbp-git-tweaks-addin.h"
#include "gbp-git-vcs.h"

struct _GbpGitTweaksAddin
{
  IdeTweaksAddin parent_instance;
  IdeVcsConfig *config;
};

G_DEFINE_FINAL_TYPE (GbpGitTweaksAddin, gbp_git_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

enum {
  PROP_0,
  PROP_AUTHOR,
  PROP_EMAIL,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_git_tweaks_addin_load (IdeTweaksAddin *addin,
                           IdeTweaks      *tweaks)
{
  GbpGitTweaksAddin *self = (GbpGitTweaksAddin *)addin;
  IdeVcsConfig *config = NULL;
  IdeContext *context = NULL;
  IdeVcs *vcs = NULL;

  g_assert (GBP_IS_GIT_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS (tweaks));

  if ((context = ide_tweaks_get_context (tweaks)) &&
      (vcs = ide_vcs_from_context (context)) &&
      GBP_IS_GIT_VCS (vcs) &&
      (config = ide_vcs_get_config (vcs)))
    {
      self->config = g_object_ref (config);
      ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                           IDE_STRV_INIT ("/plugins/git/tweaks.ui"));
    }
  else
    ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                         IDE_STRV_INIT ("/plugins/git/settings-tweaks.ui"));

  IDE_TWEAKS_ADDIN_CLASS (gbp_git_tweaks_addin_parent_class)->load (addin, tweaks);
}

static void
gbp_git_tweaks_addin_dispose (GObject *object)
{
  GbpGitTweaksAddin *self = (GbpGitTweaksAddin *)object;

  g_clear_object (&self->config);

  G_OBJECT_CLASS (gbp_git_tweaks_addin_parent_class)->dispose (object);
}

static void
gbp_git_tweaks_addin_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpGitTweaksAddin *self = GBP_GIT_TWEAKS_ADDIN (object);

  switch (prop_id)
    {
    case PROP_AUTHOR:
      ide_vcs_config_get_config (self->config, IDE_VCS_CONFIG_FULL_NAME, value);
      break;

    case PROP_EMAIL:
      ide_vcs_config_get_config (self->config, IDE_VCS_CONFIG_EMAIL, value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_tweaks_addin_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpGitTweaksAddin *self = GBP_GIT_TWEAKS_ADDIN (object);

  switch (prop_id)
    {
    case PROP_AUTHOR:
      ide_vcs_config_set_config (self->config, IDE_VCS_CONFIG_FULL_NAME, value);
      break;

    case PROP_EMAIL:
      ide_vcs_config_set_config (self->config, IDE_VCS_CONFIG_EMAIL, value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_tweaks_addin_class_init (GbpGitTweaksAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksAddinClass *tweaks_addin_class = IDE_TWEAKS_ADDIN_CLASS (klass);

  object_class->dispose = gbp_git_tweaks_addin_dispose;
  object_class->get_property = gbp_git_tweaks_addin_get_property;
  object_class->set_property = gbp_git_tweaks_addin_set_property;

  tweaks_addin_class->load = gbp_git_tweaks_addin_load;

  IDE_DEFINE_STRING_PROPERTY ("author", NULL, G_PARAM_READWRITE, AUTHOR);
  IDE_DEFINE_STRING_PROPERTY ("email", NULL, G_PARAM_READWRITE, EMAIL);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_tweaks_addin_init (GbpGitTweaksAddin *self)
{
}
