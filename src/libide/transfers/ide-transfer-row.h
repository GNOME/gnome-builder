/* ide-transfer-row.h
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gtk/gtk.h>

#include "ide-version-macros.h"

#include "transfers/ide-transfer.h"

G_BEGIN_DECLS

#define IDE_TYPE_TRANSFER_ROW (ide_transfer_row_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTransferRow, ide_transfer_row, IDE, TRANSFER_ROW, GtkListBoxRow)

IDE_AVAILABLE_IN_ALL
IdeTransfer *ide_transfer_row_get_transfer (IdeTransferRow *self);
IDE_AVAILABLE_IN_ALL
void         ide_transfer_row_set_transfer (IdeTransferRow *self,
                                            IdeTransfer    *transfer);

G_END_DECLS
