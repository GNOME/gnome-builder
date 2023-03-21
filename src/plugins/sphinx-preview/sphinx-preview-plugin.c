/* sphinx-preview-plugin.c
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

#define G_LOG_DOMAIN "sphinx-preview-plugin"

#include "config.h"

#include <libpeas.h>

#include <libide-gui.h>

#include "gbp-sphinx-preview-workspace-addin.h"

_IDE_EXTERN void
_gbp_sphinx_preview_register_types (PeasObjectModule *module)
{
  g_autofree char *path = NULL;

  if (!(path = g_find_program_in_path ("sphinx-build")))
    {
      /* We always have it in the Flatpak, so just complain for other types
       * of installations which are incomplete.
       */
      g_debug ("sphinx-build not found in PATH. Refusing to register addins.");
      return;
    }

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKSPACE_ADDIN,
                                              GBP_TYPE_SPHINX_PREVIEW_WORKSPACE_ADDIN);
}
