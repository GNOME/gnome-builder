/* ide-io.h
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
#include <libide-threading.h>

G_BEGIN_DECLS

#define IDE_IO_INSIDE
# include "ide-cached-list-model.h"
# include "ide-content-type.h"
# include "ide-directory-reaper.h"
# include "ide-file-transfer.h"
# include "ide-gfile.h"
# include "ide-heap.h"
# include "ide-line-reader.h"
# include "ide-io-enums.h"
# include "ide-marked-content.h"
# include "ide-path.h"
# include "ide-persistent-map-builder.h"
# include "ide-persistent-map.h"
# include "ide-pkcon-transfer.h"
# include "ide-pty-intercept.h"
# include "ide-recursive-file-monitor.h"
# include "ide-shell.h"
# include "ide-task-cache.h"
#undef IDE_IO_INSIDE

G_END_DECLS
