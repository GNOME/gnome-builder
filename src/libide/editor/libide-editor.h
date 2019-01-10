/* libide-editor.h
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

#include <libide-gui.h>
#include <libide-sourceview.h>

G_BEGIN_DECLS

#define IDE_EDITOR_INSIDE

#include "ide-editor-addin.h"
#include "ide-editor-page.h"
#include "ide-editor-page-addin.h"
#include "ide-editor-search.h"
#include "ide-editor-sidebar.h"
#include "ide-editor-surface.h"
#include "ide-editor-utilities.h"
#include "ide-editor-workspace.h"

#undef IDE_EDITOR_INSIDE

G_END_DECLS
