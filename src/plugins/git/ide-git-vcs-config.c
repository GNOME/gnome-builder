/* ide-git-vcs-config.c
 *
 * Copyright © 2016 Akshaya Kakkilaya <akshaya.kakkilaya@gmail.com>
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
 */

#include <libgit2-glib/ggit.h>

#include "ide-git-vcs-config.h"

struct _IdeGitVcsConfig
{
  GObject     parent_instance;

  GgitConfig *config;
};

static void vcs_config_init (IdeVcsConfigInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGitVcsConfig, ide_git_vcs_config, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_CONFIG, vcs_config_init))

IdeGitVcsConfig *
ide_git_vcs_config_new (void)
{
  return g_object_new (IDE_TYPE_GIT_VCS_CONFIG, NULL);
}

static void
ide_git_vcs_config_get_string (GgitConfig  *config,
                               const gchar *key,
                               GValue      *value,
                               GError     **error)
{
  const gchar *str;

  g_assert (GGIT_IS_CONFIG (config));
  g_assert (key != NULL);

  str = ggit_config_get_string (config, key, error);

  g_value_set_string (value, str);
}

static void
ide_git_vcs_config_set_string (GgitConfig   *config,
                               const gchar  *key,
                               const GValue *value,
                               GError      **error)
{
  const gchar *str;

  g_assert (GGIT_IS_CONFIG (config));
  g_assert (key != NULL);

  str = g_value_get_string (value);

  if (str != NULL)
    ggit_config_set_string (config, key, str, error);
}

static void
ide_git_vcs_config_get_config (IdeVcsConfig    *self,
                               IdeVcsConfigType type,
                               GValue          *value)
{
  g_autoptr(GgitConfig) config = NULL;
  GgitConfig *orig_config;

  g_return_if_fail (IDE_IS_GIT_VCS_CONFIG (self));

  orig_config = IDE_GIT_VCS_CONFIG (self)->config;
  config = ggit_config_snapshot (orig_config, NULL);

  if(config == NULL)
    return;

  switch (type)
    {
    case IDE_VCS_CONFIG_FULL_NAME:
      ide_git_vcs_config_get_string (config, "user.name", value, NULL);
      break;

    case IDE_VCS_CONFIG_EMAIL:
      ide_git_vcs_config_get_string (config, "user.email", value, NULL);
      break;

    default:
      break;
    }
}

static void
ide_git_vcs_config_set_config (IdeVcsConfig    *self,
                               IdeVcsConfigType type,
                               const GValue    *value)
{
  GgitConfig *config;

  g_return_if_fail (IDE_IS_GIT_VCS_CONFIG (self));

  config = IDE_GIT_VCS_CONFIG (self)->config;

  switch (type)
    {
    case IDE_VCS_CONFIG_FULL_NAME:
      ide_git_vcs_config_set_string (config, "user.name", value, NULL);
      break;

    case IDE_VCS_CONFIG_EMAIL:
      ide_git_vcs_config_set_string (config, "user.email", value, NULL);
      break;

    default:
      break;
    }
}

static void
ide_git_vcs_config_constructed (GObject *object)
{
  IdeGitVcsConfig *self = IDE_GIT_VCS_CONFIG (object);

  g_autoptr(GFile) global_file = NULL;

  if (!(global_file = ggit_config_find_global ()))
    {
      g_autofree gchar *path = NULL;

      path = g_build_filename (g_get_home_dir (), ".gitconfig", NULL);
      global_file = g_file_new_for_path (path);
    }

  self->config = ggit_config_new_from_file (global_file, NULL);

  G_OBJECT_CLASS (ide_git_vcs_config_parent_class)->constructed (object);
}

static void
ide_git_vcs_config_finalize (GObject *object)
{
  IdeGitVcsConfig *self = IDE_GIT_VCS_CONFIG (object);

  g_object_unref (self->config);

  G_OBJECT_CLASS (ide_git_vcs_config_parent_class)->finalize (object);
}

static void
vcs_config_init (IdeVcsConfigInterface *iface)
{
  iface->get_config = ide_git_vcs_config_get_config;
  iface->set_config = ide_git_vcs_config_set_config;
}

static void
ide_git_vcs_config_class_init (IdeGitVcsConfigClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_git_vcs_config_constructed;
  object_class->finalize = ide_git_vcs_config_finalize;
}

static void
ide_git_vcs_config_init (IdeGitVcsConfig *self)
{
}
