/* ide-projects.h
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

#define IDE_PROJECTS_INSIDE

#include "ide-doap.h"
#include "ide-doap-person.h"
#include "ide-project.h"
#include "ide-project-info.h"
#include "ide-project-file.h"
#include "ide-project-template.h"
#include "ide-project-tree-addin.h"
#include "ide-projects-global.h"
#include "ide-recent-projects.h"
#include "ide-similar-file-locator.h"
#include "ide-template-base.h"
#include "ide-template-input.h"
#include "ide-template-locator.h"
#include "ide-template-provider.h"

#undef IDE_PROJECTS_INSIDE
