/* ide-git-plugin.c
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#include <libpeas/peas.h>
#include <ide.h>

#include "ide-git-genesis-addin.h"
#include "ide-git-remote-callbacks.h"
#include "ide-git-vcs.h"
#include "ide-git-vcs-config.h"
#include "ide-git-vcs-initializer.h"

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


void
ide_git_register_types (PeasObjectModule *module)
{
  if (register_ggit ())
    {
      /* HACK: we load this type by name from the flatpak plugin, so make
       * sure it exists.
       */
      g_type_ensure (IDE_TYPE_GIT_REMOTE_CALLBACKS);

      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_VCS,
                                                  IDE_TYPE_GIT_VCS);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_VCS_CONFIG,
                                                  IDE_TYPE_GIT_VCS_CONFIG);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_VCS_INITIALIZER,
                                                  IDE_TYPE_GIT_VCS_INITIALIZER);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_GENESIS_ADDIN,
                                                  IDE_TYPE_GIT_GENESIS_ADDIN);
    }
}
