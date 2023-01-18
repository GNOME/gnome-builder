/* libide-search.h
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#define IDE_SEARCH_INSIDE
# include "ide-fuzzy-index-builder.h"
# include "ide-fuzzy-index-cursor.h"
# include "ide-fuzzy-index.h"
# include "ide-fuzzy-index-match.h"
# include "ide-fuzzy-mutable-index.h"
# include "ide-pattern-spec.h"
# include "ide-search-engine.h"
# include "ide-search-enums.h"
# include "ide-search-preview.h"
# include "ide-search-provider.h"
# include "ide-search-reducer.h"
# include "ide-search-result.h"
# include "ide-search-results.h"
#undef IDE_SEARCH_INSIDE
