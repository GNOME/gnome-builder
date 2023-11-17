/*
 * ide-terminal.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <vte/vte.h>

#include <libide-core.h>

#include "ide-terminal-palette.h"

G_BEGIN_DECLS

#define IDE_TYPE_TERMINAL (ide_terminal_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeTerminal, ide_terminal, IDE, TERMINAL, VteTerminal)

struct _IdeTerminalClass
{
  VteTerminalClass parent_class;
};

IDE_AVAILABLE_IN_46
IdeTerminalPalette *ide_terminal_get_palette (IdeTerminal         *self);
IDE_AVAILABLE_IN_46
void                ide_terminal_set_palette (IdeTerminal         *self,
                                              IdeTerminalPalette  *palette);

G_END_DECLS

