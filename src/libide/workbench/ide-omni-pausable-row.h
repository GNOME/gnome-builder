/* ide-omni-pausable-row.h
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_OMNI_PAUSABLE_ROW (ide_omni_pausable_row_get_type())

G_DECLARE_FINAL_TYPE (IdeOmniPausableRow, ide_omni_pausable_row, IDE, OMNI_PAUSABLE_ROW, GtkListBoxRow)

GtkWidget   *ide_omni_pausable_row_new          (IdePausable        *pausable);
IdePausable *ide_omni_pausable_row_get_pausable (IdeOmniPausableRow *self);
void         ide_omni_pausable_row_set_pausable (IdeOmniPausableRow *self,
                                                 IdePausable        *pausable);

G_END_DECLS
