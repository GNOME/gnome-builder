/* ide-layout-pane.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_LAYOUT_PANE_H
#define IDE_LAYOUT_PANE_H

#include <pnl.h>

G_BEGIN_DECLS

#define IDE_TYPE_LAYOUT_PANE (ide_layout_pane_get_type())

G_DECLARE_FINAL_TYPE (IdeLayoutPane, ide_layout_pane, IDE, LAYOUT_PANE, PnlDockBinEdge)

G_END_DECLS

#endif /* IDE_LAYOUT_PANE_H */
