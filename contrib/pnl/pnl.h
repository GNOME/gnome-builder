/* pnl.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef PNL_H
#define PNL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PNL_INSIDE

#include "pnl-dock.h"
#include "pnl-dock-bin.h"
#include "pnl-dock-bin-edge.h"
#include "pnl-dock-item.h"
#include "pnl-dock-manager.h"
#include "pnl-dock-overlay.h"
#include "pnl-dock-paned.h"
#include "pnl-dock-revealer.h"
#include "pnl-dock-stack.h"
#include "pnl-dock-tab-strip.h"
#include "pnl-dock-types.h"
#include "pnl-dock-widget.h"
#include "pnl-dock-window.h"

#include "pnl-version.h"

#include "pnl-tab.h"
#include "pnl-tab-strip.h"
#include "pnl-multi-paned.h"

#undef PNL_INSIDE

G_END_DECLS

#endif /* PNL_H */
