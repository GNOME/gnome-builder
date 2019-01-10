/* libide-debugger.h
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

G_BEGIN_DECLS

#define IDE_DEBUGGER_INSIDE

#include "ide-debugger-breakpoint.h"
#include "ide-debugger-breakpoints.h"
#include "ide-debugger-frame.h"
#include "ide-debugger-instruction.h"
#include "ide-debugger-library.h"
#include "ide-debugger-register.h"
#include "ide-debugger-thread-group.h"
#include "ide-debugger-thread.h"
#include "ide-debugger-types.h"
#include "ide-debugger-variable.h"
#include "ide-debugger.h"
#include "ide-debug-manager.h"

#undef IDE_DEBUGGER_INSIDE

G_END_DECLS
