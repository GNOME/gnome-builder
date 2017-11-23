/* ide-build-log-panel.c
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-log-panel"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "buildsystem/ide-build-private.h"
#include "buildui/ide-build-log-panel.h"
#include "terminal/ide-terminal.h"

struct _IdeBuildLogPanel
{
  DzlDockWidget      parent_instance;

  IdeBuildPipeline  *pipeline;
  GSettings         *settings;

  IdeTerminal       *terminal;

  guint              log_observer;
};

enum {
  PROP_0,
  PROP_PIPELINE,
  N_PROPS
};

G_DEFINE_TYPE (IdeBuildLogPanel, ide_build_log_panel, DZL_TYPE_DOCK_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ide_build_log_panel_reset_view (IdeBuildLogPanel *self)
{
  g_assert (IDE_IS_BUILD_LOG_PANEL (self));

  vte_terminal_reset (VTE_TERMINAL (self->terminal), TRUE, TRUE);
}

static void
ide_build_log_panel_log_observer (IdeBuildLogStream  stream,
                                  const gchar       *message,
                                  gssize             message_len,
                                  gpointer           user_data)
{
  IdeBuildLogPanel *self = user_data;

  g_assert (IDE_IS_BUILD_LOG_PANEL (self));
  g_assert (message != NULL);
  g_assert (message_len >= 0);
  g_assert (message[message_len] == '\0');

  vte_terminal_feed (VTE_TERMINAL (self->terminal), message, -1);
  vte_terminal_feed (VTE_TERMINAL (self->terminal), "\r\n", -1);
}

void
ide_build_log_panel_set_pipeline (IdeBuildLogPanel *self,
                                  IdeBuildPipeline *pipeline)
{
  g_return_if_fail (IDE_IS_BUILD_LOG_PANEL (self));
  g_return_if_fail (!pipeline || IDE_IS_BUILD_PIPELINE (pipeline));

  if (pipeline != self->pipeline)
    {
      if (self->pipeline != NULL)
        {
          ide_build_pipeline_remove_log_observer (self->pipeline, self->log_observer);
          self->log_observer = 0;
          g_clear_object (&self->pipeline);
          vte_terminal_set_pty (VTE_TERMINAL (self->terminal), NULL);
        }

      if (pipeline != NULL)
        {
          self->pipeline = g_object_ref (pipeline);
          self->log_observer =
            ide_build_pipeline_add_log_observer (self->pipeline,
                                                 ide_build_log_panel_log_observer,
                                                 self,
                                                 NULL);
          vte_terminal_reset (VTE_TERMINAL (self->terminal), TRUE, TRUE);
          vte_terminal_set_pty (VTE_TERMINAL (self->terminal),
                                _ide_build_pipeline_get_pty (pipeline));
        }
    }
}

static void
ide_build_log_panel_changed_font_name (IdeBuildLogPanel *self,
                                       const gchar      *key,
                                       GSettings        *settings)
{
  g_autofree gchar *font_name= NULL;
  PangoFontDescription *font_desc;

  g_assert (IDE_IS_BUILD_LOG_PANEL (self));
  g_assert (g_strcmp0 (key, "font-name") == 0);
  g_assert (G_IS_SETTINGS (settings));

  /* TODO: This probably can all go in IdeTerminal directly */

  font_name = g_settings_get_string (settings, key);
  font_desc = pango_font_description_from_string (font_name);

  if (font_desc != NULL)
    {

    }

  g_clear_pointer (&font_desc, pango_font_description_free);
}

static void
ide_build_log_panel_finalize (GObject *object)
{
  IdeBuildLogPanel *self = (IdeBuildLogPanel *)object;

  g_clear_object (&self->pipeline);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_build_log_panel_parent_class)->finalize (object);
}

static void
ide_build_log_panel_dispose (GObject *object)
{
  IdeBuildLogPanel *self = (IdeBuildLogPanel *)object;

  ide_build_log_panel_set_pipeline (self, NULL);

  G_OBJECT_CLASS (ide_build_log_panel_parent_class)->dispose (object);
}

static void
ide_build_log_panel_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeBuildLogPanel *self = IDE_BUILD_LOG_PANEL (object);

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
ide_build_log_panel_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeBuildLogPanel *self = IDE_BUILD_LOG_PANEL (object);

  switch (prop_id)
    {
    case PROP_PIPELINE:
      ide_build_log_panel_set_pipeline (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_log_panel_class_init (IdeBuildLogPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_build_log_panel_dispose;
  object_class->finalize = ide_build_log_panel_finalize;
  object_class->get_property = ide_build_log_panel_get_property;
  object_class->set_property = ide_build_log_panel_set_property;

  gtk_widget_class_set_css_name (widget_class, "buildlogpanel");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/buildui/ide-build-log-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeBuildLogPanel, terminal);

  properties [PROP_PIPELINE] =
    g_param_spec_object ("pipeline",
                         "Result",
                         "Result",
                         IDE_TYPE_BUILD_PIPELINE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_build_log_panel_clear_activate (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  IdeBuildLogPanel *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_BUILD_LOG_PANEL (self));

  ide_build_log_panel_reset_view (self);
}

static void
ide_build_log_panel_save_in_file (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  IdeBuildLogPanel *self = user_data;
  g_autoptr(GtkFileChooserNative) native = NULL;
  GtkWidget *window;
  gint res;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_BUILD_LOG_PANEL (self));

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
ide_build_log_panel_init (IdeBuildLogPanel *self)
{
  g_autoptr(GSimpleActionGroup) actions = NULL;
  static const GActionEntry entries[] = {
    { "clear", ide_build_log_panel_clear_activate },
    { "save", ide_build_log_panel_save_in_file },
  };

  gtk_widget_init_template (GTK_WIDGET (self));

#if 0
  {
    VtePty *pty = vte_pty_new_sync (VTE_PTY_DEFAULT, NULL, NULL);
    vte_terminal_set_pty (VTE_TERMINAL (self->terminal), pty);
    g_object_unref (pty);
  }
#endif

  vte_terminal_set_scrollback_lines (VTE_TERMINAL (self->terminal), 1000);
  vte_terminal_set_scroll_on_output (VTE_TERMINAL (self->terminal), FALSE);
  vte_terminal_set_scroll_on_keystroke (VTE_TERMINAL (self->terminal), TRUE);

  dzl_dock_widget_set_title (DZL_DOCK_WIDGET (self), _("Build Output"));

  ide_build_log_panel_reset_view (self);

  self->settings = g_settings_new ("org.gnome.builder.terminal");
  g_signal_connect_object (self->settings,
                           "changed::font-name",
                           G_CALLBACK (ide_build_log_panel_changed_font_name),
                           self,
                           G_CONNECT_SWAPPED);
  ide_build_log_panel_changed_font_name (self, "font-name", self->settings);

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "build-log", G_ACTION_GROUP (actions));
}
