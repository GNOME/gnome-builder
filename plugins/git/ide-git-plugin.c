/* ide-git-plugin.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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
#include "ide-git-preferences-addin.h"
#include "ide-git-vcs.h"

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
peas_register_types (PeasObjectModule *module)
{
  if (register_ggit ())
    {
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_VCS,
                                                  IDE_TYPE_GIT_VCS);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_PREFERENCES_ADDIN,
                                                  IDE_TYPE_GIT_PREFERENCES_ADDIN);
      peas_object_module_register_extension_type (module,
                                                  IDE_TYPE_GENESIS_ADDIN,
                                                  IDE_TYPE_GIT_GENESIS_ADDIN);
    }
}
