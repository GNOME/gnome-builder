/* gbp-code-index-workbench-addin.h
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

#include "ide-code-index-index.h"

G_BEGIN_DECLS

#define GBP_TYPE_CODE_INDEX_WORKBENCH_ADDIN (gbp_code_index_workbench_addin_get_type())

G_DECLARE_FINAL_TYPE (GbpCodeIndexWorkbenchAddin, gbp_code_index_workbench_addin, GBP, CODE_INDEX_WORKBENCH_ADDIN, GObject)

GbpCodeIndexWorkbenchAddin *gbp_code_index_workbench_addin_from_context (IdeContext                 *context);
void                        gbp_code_index_workbench_addin_pause        (GbpCodeIndexWorkbenchAddin *self);
void                        gbp_code_index_workbench_addin_unpause      (GbpCodeIndexWorkbenchAddin *self);
IdeCodeIndexIndex          *gbp_code_index_workbench_addin_get_index    (GbpCodeIndexWorkbenchAddin *self);

G_END_DECLS
