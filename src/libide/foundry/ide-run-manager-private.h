/* ide-run-manager-private.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-run-manager.h"

G_BEGIN_DECLS

void  _ide_run_manager_drop_caches    (IdeRunManager *self);
char *_ide_run_manager_get_default_id (IdeRunManager *self);
void  _ide_run_manager_set_default_id (IdeRunManager *self,
                                       const char    *run_command_id);

G_END_DECLS
