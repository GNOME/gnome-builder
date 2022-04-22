/* ide-editor-init.c
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

#define G_LOG_DOMAIN "ide-editor-init"

#include "config.h"

#include "ide-editor-page.h"
#include "ide-editor-private.h"
#include "ide-editor-workspace.h"

void
_ide_editor_init (void)
{
  g_type_ensure (IDE_TYPE_EDITOR_PAGE);
  g_type_ensure (IDE_TYPE_EDITOR_WORKSPACE);
}
