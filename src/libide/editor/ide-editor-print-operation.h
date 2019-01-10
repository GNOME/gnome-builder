/* ide-editor-print-operation.h
 *
 * Copyright 2015 Paolo Borelli <pborelli@gnome.org>
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

#include "ide-editor-page.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_PRINT_OPERATION (ide_editor_print_operation_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorPrintOperation, ide_editor_print_operation, IDE, EDITOR_PRINT_OPERATION, GtkPrintOperation)

IdeEditorPrintOperation *ide_editor_print_operation_new (IdeSourceView *view);

G_END_DECLS
