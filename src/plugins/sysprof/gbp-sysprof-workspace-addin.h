/* gbp-sysprof-workspace-addin.h
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

#include <libide-gui.h>

G_BEGIN_DECLS

#define GBP_TYPE_SYSPROF_WORKSPACE_ADDIN (gbp_sysprof_workspace_addin_get_type())

G_DECLARE_FINAL_TYPE (GbpSysprofWorkspaceAddin, gbp_sysprof_workspace_addin, GBP, SYSPROF_WORKSPACE_ADDIN, GObject)

G_END_DECLS
