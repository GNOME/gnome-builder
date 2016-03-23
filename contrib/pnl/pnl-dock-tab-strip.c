/* pnl-dock-tab-strip.c
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

#include "pnl-dock-tab-strip.h"

struct _PnlDockTabStrip
{
  PnlTabStrip parent;
};

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_TYPE (PnlDockTabStrip, pnl_dock_tab_strip, PNL_TYPE_TAB_STRIP)

static void
pnl_dock_tab_strip_class_init (PnlDockTabStripClass *klass)
{
}

static void
pnl_dock_tab_strip_init (PnlDockTabStrip *strip)
{
}
