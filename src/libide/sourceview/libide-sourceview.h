/* libide-sourceview.h
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

#include <libide-code.h>

G_BEGIN_DECLS

#define IDE_SOURCEVIEW_INSIDE

#include"ide-completion-context.h"
#include"ide-completion-display.h"
#include"ide-completion-proposal.h"
#include"ide-completion-list-box-row.h"
#include"ide-completion-provider.h"
#include"ide-completion-types.h"
#include"ide-completion.h"
#include"ide-hover-context.h"
#include"ide-hover-provider.h"
#include"ide-line-change-gutter-renderer.h"
#include"ide-gutter.h"
#include"ide-indenter.h"
#include"ide-snippet-chunk.h"
#include"ide-snippet-context.h"
#include"ide-snippet-parser.h"
#include"ide-snippet-storage.h"
#include"ide-snippet-types.h"
#include"ide-snippet.h"
#include"ide-source-search-context.h"
#include"ide-source-view.h"
#include"ide-text-util.h"

#define IDE_SOURCEVIEW_INSIDE

G_END_DECLS
