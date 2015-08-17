/* gb-editor-print-operation.h
 *
 * Copyright (C) 2015 Paolo Borelli <pborelli@gnome.org>
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
 */

#ifndef GB_EDITOR_PRINT_OPERATION_H
#define GB_EDITOR_PRINT_OPERATION_H

#include "gb-editor-view.h"

G_BEGIN_DECLS

#define GB_TYPE_EDITOR_PRINT_OPERATION (gb_editor_print_operation_get_type())

G_DECLARE_FINAL_TYPE (GbEditorPrintOperation, gb_editor_print_operation, GB, EDITOR_PRINT_OPERATION, GtkPrintOperation)

GbEditorPrintOperation  *gb_editor_print_operation_new    (IdeSourceView *view);

G_END_DECLS

#endif /* GB_EDITOR_PRINT_OPERATION_H */
