/* gbp-buildui-log-pane.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-buildui-log-pane"

#include "config.h"

#include <libide-terminal.h>
#include <glib/gi18n.h>

#include "ide-build-private.h"

#include "gbp-buildui-log-pane.h"

struct _GbpBuilduiLogPane
{
  IdePane            parent_instance;

  IdePipeline  *pipeline;

  GtkScrollbar      *scrollbar;
  IdeTerminal       *terminal;

  guint              log_observer;
};

enum {
  PROP_0,
  PROP_PIPELINE,
  N_PROPS
};

G_DEFINE_TYPE (GbpBuilduiLogPane, gbp_buildui_log_pane, IDE_TYPE_PANE)

static GParamSpec *properties [N_PROPS];

static void
gbp_buildui_log_pane_reset_view (GbpBuilduiLogPane *self)
{
  g_assert (GBP_IS_BUILDUI_LOG_PANE (self));

  vte_terminal_reset (VTE_TERMINAL (self->terminal), TRUE, TRUE);
}

static void
gbp_buildui_log_pane_log_observer (IdeBuildLogStream  stream,
                                   const gchar       *message,
                                   gssize             message_len,
                                   gpointer           user_data)
{
  GbpBuilduiLogPane *self = user_data;

  g_assert (GBP_IS_BUILDUI_LOG_PANE (self));
  g_assert (message != NULL);
  g_assert (message_len >= 0);
  g_assert (message[message_len] == '\0');

  vte_terminal_feed (VTE_TERMINAL (self->terminal), message, -1);
  vte_terminal_feed (VTE_TERMINAL (self->terminal), "\r\n", -1);
}

static void
gbp_buildui_log_pane_notify_pty (GbpBuilduiLogPane *self,
                                 GParamSpec        *pspec,
                                 IdePipeline       *pipeline)
{
  VtePty *pty;

  g_assert (GBP_IS_BUILDUI_LOG_PANE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  pty = ide_pipeline_get_pty (pipeline);

  if (pty != NULL)
    vte_terminal_set_pty (VTE_TERMINAL (self->terminal), pty);
}

void
gbp_buildui_log_pane_set_pipeline (GbpBuilduiLogPane *self,
                                   IdePipeline  *pipeline)
{
  g_return_if_fail (GBP_IS_BUILDUI_LOG_PANE (self));
  g_return_if_fail (!pipeline || IDE_IS_PIPELINE (pipeline));

  if (pipeline != self->pipeline)
    {
      if (self->pipeline != NULL)
        {
          g_signal_handlers_disconnect_by_func (self->pipeline,
                                                G_CALLBACK (gbp_buildui_log_pane_notify_pty),
                                                self);
          ide_pipeline_remove_log_observer (self->pipeline, self->log_observer);
          self->log_observer = 0;
          g_clear_object (&self->pipeline);
        }

      if (pipeline != NULL)
        {
          self->pipeline = g_object_ref (pipeline);
          self->log_observer =
            ide_pipeline_add_log_observer (self->pipeline,
                                                 gbp_buildui_log_pane_log_observer,
                                                 self,
                                                 NULL);
          vte_terminal_reset (VTE_TERMINAL (self->terminal), TRUE, TRUE);
          vte_terminal_set_pty (VTE_TERMINAL (self->terminal),
                                ide_pipeline_get_pty (pipeline));
          g_signal_connect_object (pipeline,
                                   "notify::pty",
                                   G_CALLBACK (gbp_buildui_log_pane_notify_pty),
                                   self,
                                   G_CONNECT_SWAPPED);
        }
    }
}

static void
gbp_buildui_log_pane_window_title_changed (GbpBuilduiLogPane *self,
                                           IdeTerminal       *terminal)
{
  g_assert (GBP_IS_BUILDUI_LOG_PANE (self));
  g_assert (VTE_IS_TERMINAL (terminal));

  if (self->pipeline != NULL)
    {
      const gchar *title;

      title = vte_terminal_get_window_title (VTE_TERMINAL (terminal));
      _ide_pipeline_set_message (self->pipeline, title);
    }
}

static void
gbp_buildui_log_pane_grab_focus (GtkWidget *widget)
{
  GbpBuilduiLogPane *self = (GbpBuilduiLogPane *)widget;

  g_assert (GBP_IS_BUILDUI_LOG_PANE (self));

  if (self->terminal != NULL)
    gtk_widget_grab_focus (GTK_WIDGET (self->terminal));
}

static void
gbp_buildui_log_pane_finalize (GObject *object)
{
  GbpBuilduiLogPane *self = (GbpBuilduiLogPane *)object;

  g_clear_object (&self->pipeline);

  G_OBJECT_CLASS (gbp_buildui_log_pane_parent_class)->finalize (object);
}

static void
gbp_buildui_log_pane_dispose (GObject *object)
{
  GbpBuilduiLogPane *self = (GbpBuilduiLogPane *)object;

  gbp_buildui_log_pane_set_pipeline (self, NULL);

  G_OBJECT_CLASS (gbp_buildui_log_pane_parent_class)->dispose (object);
}

static void
gbp_buildui_log_pane_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpBuilduiLogPane *self = GBP_BUILDUI_LOG_PANE (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      g_value_set_object (value, self->pipeline);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_log_pane_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpBuilduiLogPane *self = GBP_BUILDUI_LOG_PANE (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      gbp_buildui_log_pane_set_pipeline (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_buildui_log_pane_class_init (GbpBuilduiLogPaneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_buildui_log_pane_dispose;
  object_class->finalize = gbp_buildui_log_pane_finalize;
  object_class->get_property = gbp_buildui_log_pane_get_property;
  object_class->set_property = gbp_buildui_log_pane_set_property;

  widget_class->grab_focus = gbp_buildui_log_pane_grab_focus;

  gtk_widget_class_set_css_name (widget_class, "buildlogpanel");
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/buildui/gbp-buildui-log-pane.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiLogPane, scrollbar);
  gtk_widget_class_bind_template_child (widget_class, GbpBuilduiLogPane, terminal);

  properties [PROP_PIPELINE] =
    g_param_spec_object ("pipeline",
                         "Result",
                         "Result",
                         IDE_TYPE_PIPELINE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_buildui_log_pane_clear_activate (GSimpleAction *action,
                                     GVariant      *param,
                                     gpointer       user_data)
{
  GbpBuilduiLogPane *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILDUI_LOG_PANE (self));

  gbp_buildui_log_pane_reset_view (self);
}

static void
gbp_buildui_log_pane_save_in_file (GSimpleAction *action,
                                   GVariant      *param,
                                   gpointer       user_data)
{
  GbpBuilduiLogPane *self = user_data;
  g_autoptr(GtkFileChooserNative) native = NULL;
  GtkWidget *window;
  gint res;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_BUILDUI_LOG_PANE (self));

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
terminal_size_allocate (GbpBuilduiLogPane *self,
                        GtkAllocation     *allocation,
                        IdeTerminal       *terminal)
{
  VtePty *pty;
  gint rows = 0;
  gint columns = 0;

  g_assert (GBP_IS_BUILDUI_LOG_PANE (self));
  g_assert (allocation != NULL);
  g_assert (IDE_IS_TERMINAL (terminal));

  pty = vte_terminal_get_pty (VTE_TERMINAL (self->terminal));

  if (self->pipeline != NULL && pty != NULL)
    {
      if (vte_pty_get_size (pty, &rows, &columns, NULL))
        _ide_pipeline_set_pty_size (self->pipeline, rows, columns);
    }
}

static void
gbp_buildui_log_pane_init (GbpBuilduiLogPane *self)
{
  g_autoptr(GSimpleActionGroup) actions = NULL;
  static const GActionEntry entries[] = {
    { "clear", gbp_buildui_log_pane_clear_activate },
    { "save", gbp_buildui_log_pane_save_in_file },
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  dzl_dock_widget_set_icon_name (DZL_DOCK_WIDGET (self), "builder-build-symbolic");

  g_signal_connect_object (self->terminal,
                           "size-allocate",
                           G_CALLBACK (terminal_size_allocate),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_signal_connect_object (self->terminal,
                           "window-title-changed",
                           G_CALLBACK (gbp_buildui_log_pane_window_title_changed),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_range_set_adjustment (GTK_RANGE (self->scrollbar),
                            gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->terminal)));

  vte_terminal_set_scrollback_lines (VTE_TERMINAL (self->terminal), 1000);
  vte_terminal_set_scroll_on_output (VTE_TERMINAL (self->terminal), FALSE);
  vte_terminal_set_scroll_on_keystroke (VTE_TERMINAL (self->terminal), TRUE);

  dzl_dock_widget_set_title (DZL_DOCK_WIDGET (self), _("Build Output"));

  gbp_buildui_log_pane_reset_view (self);

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "build-log", G_ACTION_GROUP (actions));
}
