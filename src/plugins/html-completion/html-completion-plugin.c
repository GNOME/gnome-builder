/* html-completion-plugin.c
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

#include "config.h"

#include <libpeas.h>
#include <gtksourceview/gtksource.h>

#include "ide-html-completion-provider.h"

_IDE_EXTERN void
_ide_html_completion_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                              IDE_TYPE_HTML_COMPLETION_PROVIDER);
}
