/* ide-pkcon-transfer.h
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#include "ide-version-macros.h"

#include "transfers/ide-transfer.h"

G_BEGIN_DECLS

#define IDE_TYPE_PKCON_TRANSFER (ide_pkcon_transfer_get_type())

G_DECLARE_FINAL_TYPE (IdePkconTransfer, ide_pkcon_transfer, IDE, PKCON_TRANSFER, IdeTransfer)

IDE_AVAILABLE_IN_ALL
IdePkconTransfer *ide_pkcon_transfer_new (const gchar * const *packages);

G_END_DECLS
