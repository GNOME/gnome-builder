/* git-plugin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "git-plugin"

#include "config.h"

#include <libpeas/peas.h>
#include <libgit2-glib/ggit.h>
#include <libide-editor.h>
#include <libide-foundry.h>
#include <libide-vcs.h>

#include "gbp-git-buffer-addin.h"
#include "gbp-git-dependency-updater.h"
#include "gbp-git-pipeline-addin.h"
#include "gbp-git-remote-callbacks.h"
#include "gbp-git-vcs-cloner.h"
#include "gbp-git-vcs-config.h"
#include "gbp-git-vcs-initializer.h"
#include "gbp-git-workbench-addin.h"

static gboolean
register_ggit (void)
{
  GgitFeatureFlags ggit_flags;

  ggit_init ();

  ggit_flags = ggit_get_features ();

  if ((ggit_flags & GGIT_FEATURE_THREADS) == 0)
    {
      g_printerr ("Builder requires libgit2-glib with threading support.");
      return FALSE;
    }

  if ((ggit_flags & GGIT_FEATURE_SSH) == 0)
    {
      g_printerr ("Builder requires libgit2-glib with SSH support.");
      return FALSE;
    }

  return TRUE;
}


_IDE_EXTERN void
_gbp_git_register_types (PeasObjectModule *module)
{
  if (register_ggit ())
    {
      ide_g_file_add_ignored_pattern (".git");

      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_BUFFER_ADDIN,
                                                  GBP_TYPE_GIT_BUFFER_ADDIN);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_BUILD_PIPELINE_ADDIN,
                                                  GBP_TYPE_GIT_PIPELINE_ADDIN);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_DEPENDENCY_UPDATER,
                                                  GBP_TYPE_GIT_DEPENDENCY_UPDATER);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_VCS_CONFIG,
                                                  GBP_TYPE_GIT_VCS_CONFIG);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_VCS_CLONER,
                                                  GBP_TYPE_GIT_VCS_CLONER);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_VCS_INITIALIZER,
                                                  GBP_TYPE_GIT_VCS_INITIALIZER);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_WORKBENCH_ADDIN,
                                                  GBP_TYPE_GIT_WORKBENCH_ADDIN);

      g_type_ensure (GBP_TYPE_GIT_REMOTE_CALLBACKS);
    }
}
