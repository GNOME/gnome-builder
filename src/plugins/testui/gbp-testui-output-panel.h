/* gbp-testui-output-panel.h
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

#include <vte/vte.h>

#include <libide-gui.h>

G_BEGIN_DECLS

#define GBP_TYPE_TESTUI_OUTPUT_PANEL (gbp_testui_output_panel_get_type())

G_DECLARE_FINAL_TYPE (GbpTestuiOutputPanel, gbp_testui_output_panel, GBP, TESTUI_OUTPUT_PANEL, IdePane)

GbpTestuiOutputPanel *gbp_testui_output_panel_new   (VtePty               *pty);
void                  gbp_testui_output_panel_reset (GbpTestuiOutputPanel *self);
void                  gbp_testui_output_panel_write (GbpTestuiOutputPanel *self,
                                                     const char           *message);

G_END_DECLS
