/* gbp-test-output-panel.c
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

#define G_LOG_DOMAIN "gbp-test-output-panel"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-gui.h>
#include <libide-editor.h>
#include <libide-terminal.h>

#include "gbp-test-output-panel.h"

struct _GbpTestOutputPanel
{
  DzlDockWidget    parent_instance;
  IdeTerminalPage *terminal;
};

G_DEFINE_TYPE (GbpTestOutputPanel, gbp_test_output_panel, DZL_TYPE_DOCK_WIDGET)

static void
gbp_test_output_panel_class_init (GbpTestOutputPanelClass *klass)
{
}

static void
gbp_test_output_panel_init (GbpTestOutputPanel *self)
{
  dzl_dock_widget_set_title (DZL_DOCK_WIDGET (self), _("Unit Test Output"));
  dzl_dock_widget_set_icon_name (DZL_DOCK_WIDGET (self), "builder-unit-tests-symbolic");

  self->terminal = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                                 "manage-spawn", FALSE,
                                 "visible", TRUE,
                                 NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->terminal));
}

GtkWidget *
gbp_test_output_panel_new (VtePty *pty)
{
  GbpTestOutputPanel *self;

  g_return_val_if_fail (VTE_IS_PTY (pty), NULL);

  self = g_object_new (GBP_TYPE_TEST_OUTPUT_PANEL, NULL);
  ide_terminal_page_set_pty (self->terminal, pty);

  return GTK_WIDGET (self);
}
