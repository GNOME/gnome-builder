/* gbp-testui-output-panel.c
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

#define G_LOG_DOMAIN "gbp-testui-output-panel"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-terminal.h>

#include "gbp-testui-output-panel.h"

struct _GbpTestuiOutputPanel
{
  IdePane       parent_instance;
  IdeTerminal  *terminal;
};

G_DEFINE_FINAL_TYPE (GbpTestuiOutputPanel, gbp_testui_output_panel, IDE_TYPE_PANE)

static void
gbp_testui_output_panel_save_in_file_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GtkFileDialog *dialog = (GtkFileDialog *)object;
  g_autoptr(GbpTestuiOutputPanel) self = user_data;
  g_autoptr(GFileOutputStream) stream = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = user_data;

  IDE_ENTRY;

  g_assert (GTK_IS_FILE_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_TESTUI_OUTPUT_PANEL (self));

  if (!(file = gtk_file_dialog_save_finish (dialog, result, &error)))
    return;

  stream = g_file_replace (file,
                           NULL,
                           FALSE,
                           G_FILE_CREATE_REPLACE_DESTINATION,
                           NULL,
                           &error);

  if (stream != NULL)
    {
      vte_terminal_write_contents_sync (VTE_TERMINAL (self->terminal),
                                        G_OUTPUT_STREAM (stream),
                                        VTE_WRITE_DEFAULT,
                                        NULL,
                                        &error);
      g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL);
    }

  if (error != NULL)
    g_warning ("Failed to write contents: %s", error->message);

  IDE_EXIT;
}

static void
gbp_testui_output_panel_save_in_file (GtkWidget  *widget,
                                      const char *action_name,
                                      GVariant   *param)
{
  GbpTestuiOutputPanel *self = (GbpTestuiOutputPanel *)widget;
  g_autoptr(GtkFileDialog) dialog = NULL;
  GtkWidget *window;

  IDE_ENTRY;

  g_assert (GBP_IS_TESTUI_OUTPUT_PANEL (self));

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Save File"));
  gtk_file_dialog_set_accept_label (dialog, _("Save"));

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (window),
                        NULL,
                        gbp_testui_output_panel_save_in_file_cb,
                        g_object_ref (self));

  IDE_EXIT;

}

static void
gbp_testui_output_panel_clear_activate (GtkWidget  *widget,
                                        const char *action_name,
                                        GVariant   *param)
{
  GbpTestuiOutputPanel *self = (GbpTestuiOutputPanel *)widget;

  g_assert (GBP_IS_TESTUI_OUTPUT_PANEL (self));

  vte_terminal_reset (VTE_TERMINAL (self->terminal), TRUE, TRUE);
}

static void
gbp_testui_output_panel_class_init (GbpTestuiOutputPanelClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass*)klass;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/testui/gbp-testui-output-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpTestuiOutputPanel, terminal);

  gtk_widget_class_install_action (widget_class, "test-output.clear", NULL, gbp_testui_output_panel_clear_activate);
  gtk_widget_class_install_action (widget_class, "test-output.save", NULL, gbp_testui_output_panel_save_in_file);
}

static void
gbp_testui_output_panel_init (GbpTestuiOutputPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET(self));

  panel_widget_set_title (PANEL_WIDGET (self), _("Unit Test Output"));
  panel_widget_set_icon_name (PANEL_WIDGET (self), "builder-unit-tests-symbolic");
}

GbpTestuiOutputPanel *
gbp_testui_output_panel_new (VtePty *pty)
{
  GbpTestuiOutputPanel *self;

  g_return_val_if_fail (VTE_IS_PTY (pty), NULL);

  self = g_object_new (GBP_TYPE_TESTUI_OUTPUT_PANEL, NULL);
  vte_terminal_set_pty (VTE_TERMINAL (self->terminal), pty);

  return self;
}

void
gbp_testui_output_panel_reset (GbpTestuiOutputPanel *self)
{
  g_return_if_fail (GBP_IS_TESTUI_OUTPUT_PANEL (self));

  vte_terminal_reset (VTE_TERMINAL (self->terminal), TRUE, TRUE);
}

void
gbp_testui_output_panel_write (GbpTestuiOutputPanel *self,
                               const char           *message)
{
  g_return_if_fail (GBP_IS_TESTUI_OUTPUT_PANEL (self));
  g_return_if_fail (message != NULL);

  vte_terminal_feed (VTE_TERMINAL (self->terminal), message, -1);
  vte_terminal_feed (VTE_TERMINAL (self->terminal), "\r\n", -1);
}
