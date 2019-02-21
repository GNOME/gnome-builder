/* gbp-test-output-panel.h
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

#include <dazzle.h>
#include <vte/vte.h>

G_BEGIN_DECLS

#define GBP_TYPE_TEST_OUTPUT_PANEL (gbp_test_output_panel_get_type())

G_DECLARE_FINAL_TYPE (GbpTestOutputPanel, gbp_test_output_panel, GBP, TEST_OUTPUT_PANEL, DzlDockWidget)

GtkWidget *gbp_test_output_panel_new (VtePty *pty);

G_END_DECLS
