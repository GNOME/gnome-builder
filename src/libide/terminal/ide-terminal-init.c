/* ide-terminal-init.c
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

#define G_LOG_DOMAIN "ide-terminal-init"

#include "ide-terminal.h"
#include "ide-terminal-launcher.h"
#include "ide-terminal-page.h"
#include "ide-terminal-private.h"
#include "ide-terminal-search.h"

void
_ide_terminal_init (void)
{
  g_type_ensure (IDE_TYPE_TERMINAL);
  g_type_ensure (IDE_TYPE_TERMINAL_LAUNCHER);
  g_type_ensure (IDE_TYPE_TERMINAL_PAGE);
  g_type_ensure (IDE_TYPE_TERMINAL_SEARCH);
}
