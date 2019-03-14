/* ide-transfer-manager-private.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-transfer.h"
#include "ide-transfer-manager.h"

G_BEGIN_DECLS

GActionGroup *_ide_transfer_manager_get_actions  (IdeTransferManager *self);
void          _ide_transfer_manager_cancel_by_id (IdeTransferManager *self,
                                                  gint                unique_id);
gint          _ide_transfer_get_id               (IdeTransfer        *self);

G_END_DECLS
