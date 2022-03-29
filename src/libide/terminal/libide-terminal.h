/* libide-terminal.h
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

#pragma once

#include <libide-core.h>
#include <libide-io.h>
#include <libide-gui.h>
#include <libide-threading.h>
#include <vte/vte.h>

#define IDE_TERMINAL_INSIDE

#include "ide-terminal.h"
#include "ide-terminal-launcher.h"
#include "ide-terminal-page.h"
#include "ide-terminal-popover.h"
#include "ide-terminal-search.h"
#include "ide-terminal-util.h"

#undef IDE_TERMINAL_INSIDE
