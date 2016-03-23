/* gbp-build-log-panel.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>
#include <ide.h>

#include "util/ide-pango.h"

#include "egg-signal-group.h"

#include "gbp-build-log-panel.h"

struct _GbpBuildLogPanel
{
  PnlDockWidget      parent_instance;

  IdeBuildResult    *result;
  EggSignalGroup    *signals;
  GtkCssProvider    *css;
  GSettings         *settings;
  GtkTextBuffer     *buffer;

  GtkScrolledWindow *scroller;
  GtkTextView       *text_view;
  GtkTextTag        *stderr_tag;
};

enum {
  PROP_0,
  PROP_RESULT,
  LAST_PROP
};

G_DEFINE_TYPE (GbpBuildLogPanel, gbp_build_log_panel, PNL_TYPE_DOCK_WIDGET)

static GParamSpec *properties [LAST_PROP];

static void
gbp_build_log_panel_reset_view (GbpBuildLogPanel *self)
{
  GtkStyleContext *context;

  g_assert (GBP_IS_BUILD_LOG_PANEL (self));

  g_clear_object (&self->buffer);

  if (self->text_view != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->text_view));

  self->buffer = gtk_text_buffer_new (NULL);
  self->stderr_tag = gtk_text_buffer_create_tag (self->buffer,
                                                 "stderr-tag",
                                                 "foreground", "#ff0000",
                                                 "weight", PANGO_WEIGHT_BOLD,
                                                 NULL);

  self->text_view = g_object_new (GTK_TYPE_TEXT_VIEW,
                                  "buffer", self->buffer,
                                  "monospace", TRUE,
                                  "visible", TRUE,
                                  NULL);
  context = gtk_widget_get_style_context (GTK_WIDGET (self->text_view));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (self->css),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_container_add (GTK_CONTAINER (self->scroller), GTK_WIDGET (self->text_view));
}

static void
gbp_build_log_panel_log (GbpBuildLogPanel  *self,
                         IdeBuildResultLog  log,
                         const gchar       *message,
                         IdeBuildResult    *result)
{
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GBP_IS_BUILD_LOG_PANEL (self));
  g_assert (message != NULL);
  g_assert (IDE_IS_BUILD_RESULT (result));

  gtk_text_buffer_get_end_iter (self->buffer, &iter);

  if (G_LIKELY (log == IDE_BUILD_RESULT_LOG_STDOUT))
    {
      gtk_text_buffer_insert (self->buffer, &iter, message, -1);
    }
  else
    {
      GtkTextIter begin;
      guint offset;

      offset = gtk_text_iter_get_offset (&iter);
      gtk_text_buffer_insert (self->buffer, &iter, message, -1);
      gtk_text_buffer_get_iter_at_offset (self->buffer, &begin, offset);
      gtk_text_buffer_apply_tag (self->buffer, self->stderr_tag, &begin, &iter);
    }

  insert = gtk_text_buffer_get_insert (self->buffer);
  gtk_text_view_scroll_to_mark (self->text_view, insert, 0.0, TRUE, 0.0, 0.0);
}

void
gbp_build_log_panel_set_result (GbpBuildLogPanel *self,
                                IdeBuildResult   *result)
{
  g_return_if_fail (GBP_IS_BUILD_LOG_PANEL (self));
  g_return_if_fail (!result || IDE_IS_BUILD_RESULT (result));

  if (g_set_object (&self->result, result))
    {
      gbp_build_log_panel_reset_view (self);
      egg_signal_group_set_target (self->signals, result);
    }
}

static void
gbp_build_log_panel_changed_font_name (GbpBuildLogPanel *self,
                                       const gchar      *key,
                                       GSettings        *settings)
{
  gchar *font_name;
  PangoFontDescription *font_desc;

  g_assert (GBP_IS_BUILD_LOG_PANEL (self));
  g_assert (g_strcmp0 (key, "font-name") == 0);
  g_assert (G_IS_SETTINGS (settings));

  font_name = g_settings_get_string (settings, key);
  font_desc = pango_font_description_from_string (font_name);

  if (font_desc != NULL)
    {
      gchar *fragment;
      gchar *css;

      fragment = ide_pango_font_description_to_css (font_desc);
      css = g_strdup_printf ("textview { %s }", fragment);

      gtk_css_provider_load_from_data (self->css, css, -1, NULL);

      pango_font_description_free (font_desc);
      g_free (fragment);
      g_free (css);
    }

  g_free (font_name);
}

static void
gbp_build_log_panel_finalize (GObject *object)
{
  GbpBuildLogPanel *self = (GbpBuildLogPanel *)object;

  self->stderr_tag = NULL;

  g_clear_object (&self->result);
  g_clear_object (&self->signals);
  g_clear_object (&self->css);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gbp_build_log_panel_parent_class)->finalize (object);
}

static void
gbp_build_log_panel_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpBuildLogPanel *self = GBP_BUILD_LOG_PANEL (object);

  switch (prop_id)
    {
    case PROP_RESULT:
      g_value_set_object (value, self->result);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_build_log_panel_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpBuildLogPanel *self = GBP_BUILD_LOG_PANEL (object);

  switch (prop_id)
    {
    case PROP_RESULT:
      gbp_build_log_panel_set_result (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_build_log_panel_class_init (GbpBuildLogPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_build_log_panel_finalize;
  object_class->get_property = gbp_build_log_panel_get_property;
  object_class->set_property = gbp_build_log_panel_set_property;

  gtk_widget_class_set_css_name (widget_class, "buildlogpanel");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/build-tools-plugin/gbp-build-log-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpBuildLogPanel, scroller);

  properties [PROP_RESULT] =
    g_param_spec_object ("result",
                         "Result",
                         "Result",
                         IDE_TYPE_BUILD_RESULT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gbp_build_log_panel_init (GbpBuildLogPanel *self)
{
  self->css = gtk_css_provider_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_set (self, "title", _("Build Output"), NULL);

  gbp_build_log_panel_reset_view (self);

  self->signals = egg_signal_group_new (IDE_TYPE_BUILD_RESULT);

  egg_signal_group_connect_object (self->signals,
                                   "log",
                                   G_CALLBACK (gbp_build_log_panel_log),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->settings = g_settings_new ("org.gnome.builder.terminal");
  g_signal_connect_object (self->settings,
                           "changed::font-name",
                           G_CALLBACK (gbp_build_log_panel_changed_font_name),
                           self,
                           G_CONNECT_SWAPPED);
  gbp_build_log_panel_changed_font_name (self, "font-name", self->settings);
}
