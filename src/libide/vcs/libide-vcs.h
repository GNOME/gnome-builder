/* ide-vcs.h
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
#include <libide-io.h>

#define IDE_VCS_INSIDE

#include "ide-directory-vcs.h"
#include "ide-vcs-branch.h"
#include "ide-vcs-cloner.h"
#include "ide-vcs-clone-request.h"
#include "ide-vcs-config.h"
#include "ide-vcs-enums.h"
#include "ide-vcs-initializer.h"
#include "ide-vcs-uri.h"
#include "ide-vcs-file-info.h"
#include "ide-vcs.h"
#include "ide-vcs-monitor.h"
#include "ide-vcs-tag.h"

#undef IDE_VCS_INSIDE
