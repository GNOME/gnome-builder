/* gb-editor-settings-widget.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-editor-settings-widget.h"

struct _GbEditorSettingsWidgetPrivate
{
  GSettings      *settings;
  gchar          *language;

  GtkCheckButton *auto_indent;
  GtkCheckButton *highlight_current_line;
  GtkCheckButton *highlight_matching_brackets;
  GtkCheckButton *insert_spaces_instead_of_tabs;
  GtkCheckButton *show_line_marks;
  GtkCheckButton *show_line_numbers;
  GtkCheckButton *show_right_margin;
  GtkCheckButton *smart_home_end;
  GtkSpinButton  *right_margin_position;
  GtkSpinButton  *tab_width;
  GtkCheckButton *trim_trailing_whitespace;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorSettingsWidget, gb_editor_settings_widget,
                            GTK_TYPE_GRID)

enum {
  PROP_0,
  PROP_LANGUAGE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

const gchar *
gb_editor_settings_widget_get_language (GbEditorSettingsWidget *widget)
{
  g_return_val_if_fail (GB_IS_EDITOR_SETTINGS_WIDGET (widget), NULL);

  return widget->priv->language;
}

void
gb_editor_settings_widget_set_language (GbEditorSettingsWidget *widget,
                                        const gchar            *language)
{
  GbEditorSettingsWidgetPrivate *priv;

  g_return_if_fail (GB_IS_EDITOR_SETTINGS_WIDGET (widget));

  priv = widget->priv;

  if (language != priv->language)
    {
      gchar *path;

      g_free (priv->language);
      priv->language = g_strdup (language);

      g_clear_object (&priv->settings);

      path = g_strdup_printf ("/org/gnome/builder/editor/language/%s/",
                              language);
      priv->settings = g_settings_new_with_path (
        "org.gnome.builder.editor.language", path);
      g_free (path);

      g_settings_bind (priv->settings, "auto-indent",
                       priv->auto_indent, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (priv->settings, "highlight-current-line",
                       priv->highlight_current_line, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (priv->settings, "highlight-matching-brackets",
                       priv->highlight_matching_brackets, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (priv->settings, "insert-spaces-instead-of-tabs",
                       priv->insert_spaces_instead_of_tabs, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (priv->settings, "show-line-marks",
                       priv->show_line_marks, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (priv->settings, "show-line-numbers",
                       priv->show_line_numbers, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (priv->settings, "show-right-margin",
                       priv->show_right_margin, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (priv->settings, "smart-home-end",
                       priv->smart_home_end, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (priv->settings, "right-margin-position",
                       priv->right_margin_position, "value",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (priv->settings, "tab-width",
                       priv->tab_width, "value",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (priv->settings, "trim-trailing-whitespace",
                       priv->trim_trailing_whitespace, "active",
                       G_SETTINGS_BIND_DEFAULT);

      g_object_notify_by_pspec (G_OBJECT (widget), gParamSpecs [PROP_LANGUAGE]);
    }
}

static void
gb_editor_settings_widget_finalize (GObject *object)
{
  GbEditorSettingsWidgetPrivate *priv = GB_EDITOR_SETTINGS_WIDGET (object)->priv;

  g_clear_pointer (&priv->language, g_free);
  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (gb_editor_settings_widget_parent_class)->finalize (object);
}

static void
gb_editor_settings_widget_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbEditorSettingsWidget *self = GB_EDITOR_SETTINGS_WIDGET (object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      g_value_set_string (value, gb_editor_settings_widget_get_language (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_settings_widget_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GbEditorSettingsWidget *self = GB_EDITOR_SETTINGS_WIDGET (object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      gb_editor_settings_widget_set_language (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_settings_widget_class_init (GbEditorSettingsWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_editor_settings_widget_finalize;
  object_class->get_property = gb_editor_settings_widget_get_property;
  object_class->set_property = gb_editor_settings_widget_set_property;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-editor-settings-widget.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, auto_indent);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, highlight_current_line);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, highlight_matching_brackets);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, insert_spaces_instead_of_tabs);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, right_margin_position);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, show_line_marks);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, show_line_numbers);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, show_right_margin);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, smart_home_end);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, tab_width);
  gtk_widget_class_bind_template_child_private (widget_class, GbEditorSettingsWidget, trim_trailing_whitespace);

  gParamSpecs [PROP_LANGUAGE] =
    g_param_spec_string ("language",
                         _("Language"),
                         _("The language to change the settings for."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LANGUAGE,
                                   gParamSpecs [PROP_LANGUAGE]);
}

static void
gb_editor_settings_widget_init (GbEditorSettingsWidget *self)
{
  self->priv = gb_editor_settings_widget_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
