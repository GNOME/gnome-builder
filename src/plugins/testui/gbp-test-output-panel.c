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
#include <libide-terminal.h>

#include "gbp-test-output-panel.h"

struct _GbpTestOutputPanel
{
  IdePane       parent_instance;
  IdeTerminal  *terminal;
  GtkScrollbar *scrollbar;
};

G_DEFINE_TYPE (GbpTestOutputPanel, gbp_test_output_panel, IDE_TYPE_PANE)

static void
gbp_test_output_panel_class_init (GbpTestOutputPanelClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass*)klass;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/testui/gbp-test-output-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpTestOutputPanel, scrollbar);
  gtk_widget_class_bind_template_child (widget_class, GbpTestOutputPanel, terminal);
}

static void
gbp_testui_output_panel_save_in_file (GSimpleAction *action,
                                      GVariant      *param,
                                      gpointer       user_data)
{
  GbpTestOutputPanel *self = user_data;
  g_autoptr(GtkFileChooserNative) native = NULL;
  GtkWidget *window;
  gint res;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_TEST_OUTPUT_PANEL (self));

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  native = gtk_file_chooser_native_new (_("Save File"),
                                        GTK_WINDOW (window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Save"),
                                        _("_Cancel"));

  res = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));

  if (res == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = NULL;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));

      if (file != NULL)
        {
          g_autoptr(GFileOutputStream) stream = NULL;
          g_autoptr(GError) error = NULL;

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
        }
    }

  IDE_EXIT;

}

static void
gbp_testui_output_panel_clear_activate (GSimpleAction *action,
                                        GVariant      *param,
                                        gpointer       user_data)
{
  GbpTestOutputPanel *self = user_data;

  g_assert (GBP_IS_TEST_OUTPUT_PANEL (self));
  g_assert (G_IS_SIMPLE_ACTION (action));

  vte_terminal_reset (VTE_TERMINAL (self->terminal), TRUE, TRUE);
}

static void
gbp_test_output_panel_init (GbpTestOutputPanel *self)
{
  static const GActionEntry entries[] = {
    { "clear", gbp_testui_output_panel_clear_activate },
    { "save", gbp_testui_output_panel_save_in_file },
  };
  g_autoptr(GSimpleActionGroup) actions = NULL;
  GtkAdjustment *vadj;

  gtk_widget_init_template (GTK_WIDGET(self));

  dzl_dock_widget_set_title (DZL_DOCK_WIDGET (self), _("Unit Test Output"));
  dzl_dock_widget_set_icon_name (DZL_DOCK_WIDGET (self), "builder-unit-tests-symbolic");

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "test-output", G_ACTION_GROUP (actions));

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->terminal));
  gtk_range_set_adjustment (GTK_RANGE (self->scrollbar), vadj);
}

GtkWidget *
gbp_test_output_panel_new (VtePty *pty)
{
  GbpTestOutputPanel *self;

  g_return_val_if_fail (VTE_IS_PTY (pty), NULL);

  self = g_object_new (GBP_TYPE_TEST_OUTPUT_PANEL, NULL);
  vte_terminal_set_pty (VTE_TERMINAL (self->terminal), pty);

  return GTK_WIDGET (self);
}
