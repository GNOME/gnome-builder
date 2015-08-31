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
#include <gtksourceview/gtksource.h>

#include "gb-editor-settings-widget.h"
#include "gb-widget.h"

struct _GbEditorSettingsWidget
{
  GtkGrid         parent_instance;

  GSettings      *settings;
  gchar          *language;

  GtkCheckButton *auto_indent;
  GtkCheckButton *insert_matching_brace;
  GtkCheckButton *insert_spaces_instead_of_tabs;
  GtkCheckButton *overwrite_braces;
  GtkCheckButton *show_right_margin;
  GtkListBox     *snippets;
  GtkBox         *snippets_container;
  GtkSpinButton  *right_margin_position;
  GtkSpinButton  *tab_width;
  GtkCheckButton *trim_trailing_whitespace;
};

G_DEFINE_TYPE (GbEditorSettingsWidget, gb_editor_settings_widget, GTK_TYPE_GRID)

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

  return widget->language;
}

static void
foreach_cb (gpointer data,
            gpointer user_data)
{
  GtkListBoxRow *row;
  GtkListBox *box = user_data;
  IdeSourceSnippet *snippet = data;
  GtkBox *hbox;
  GtkLabel *label;
  const gchar *trigger;
  const gchar *desc;

  g_assert (GTK_IS_LIST_BOX (box));

  trigger = ide_source_snippet_get_trigger (snippet);
  desc = ide_source_snippet_get_description (snippet);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data_full (G_OBJECT (row), "SNIPPET", g_object_ref (snippet), g_object_unref);
  hbox = g_object_new (GTK_TYPE_BOX,
                       "visible", TRUE,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       NULL);
  label = g_object_new (GTK_TYPE_LABEL,
                        "label", trigger,
                        "hexpand", TRUE,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (label));
  label = g_object_new (GTK_TYPE_LABEL,
                        "label", desc,
                        "visible", TRUE,
                        "xalign", 1.0f,
                        NULL);
  gb_widget_add_style_class (GTK_WIDGET (label), "dim-label");
  gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (label));
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (hbox));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (row));
}

static void
load_snippets_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeSourceSnippetsManager *sm = (IdeSourceSnippetsManager *)object;
  g_autoptr(GbEditorSettingsWidget) self = user_data;
  IdeSourceSnippets *snippets;

  if (!ide_source_snippets_manager_load_finish (sm, result, NULL))
    return;

  snippets = ide_source_snippets_manager_get_for_language_id (sm, self->language);
  if (snippets == NULL)
    return;

  ide_source_snippets_foreach (snippets, NULL, foreach_cb, self->snippets);

  if (ide_source_snippets_count (snippets) > 0)
    gtk_widget_show (GTK_WIDGET (self->snippets_container));
}

void
gb_editor_settings_widget_set_language (GbEditorSettingsWidget *widget,
                                        const gchar            *language)
{
  g_return_if_fail (GB_IS_EDITOR_SETTINGS_WIDGET (widget));

  if (!ide_str_equal0 (language, widget->language))
    {
      IdeSourceSnippetsManager *sm;
      gchar *path;

      g_free (widget->language);
      widget->language = g_strdup (language);

      g_clear_object (&widget->settings);

      path = g_strdup_printf ("/org/gnome/builder/editor/language/%s/",
                              language);
      widget->settings = g_settings_new_with_path (
        "org.gnome.builder.editor.language", path);
      g_free (path);

      g_settings_bind (widget->settings, "auto-indent",
                       widget->auto_indent, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (widget->settings, "insert-matching-brace",
                       widget->insert_matching_brace, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (widget->settings, "insert-spaces-instead-of-tabs",
                       widget->insert_spaces_instead_of_tabs, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (widget->settings, "overwrite-braces",
                       widget->overwrite_braces, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (widget->settings, "show-right-margin",
                       widget->show_right_margin, "active",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (widget->settings, "right-margin-position",
                       widget->right_margin_position, "value",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (widget->settings, "tab-width",
                       widget->tab_width, "value",
                       G_SETTINGS_BIND_DEFAULT);
      g_settings_bind (widget->settings, "trim-trailing-whitespace",
                       widget->trim_trailing_whitespace, "active",
                       G_SETTINGS_BIND_DEFAULT);

      sm = g_object_new (IDE_TYPE_SOURCE_SNIPPETS_MANAGER, NULL);
      ide_source_snippets_manager_load_async (sm, NULL, load_snippets_cb, g_object_ref (widget));

      g_object_notify_by_pspec (G_OBJECT (widget), gParamSpecs [PROP_LANGUAGE]);
    }
}

static gboolean
transform_title_func (GBinding     *binding,
                      const GValue *from_value,
                      GValue       *to_value,
                      gpointer      user_data)
{
  gchar *title;

  title = g_strdup_printf (_("%s (read-only)"), g_value_get_string (from_value));
  g_value_take_string (to_value, title);

  return TRUE;
}

static void
snippet_activated_cb (GbEditorSettingsWidget *self,
                      GtkListBoxRow          *row,
                      GtkListBox             *list_box)
{
  GtkWindow *window;
  GtkWidget *toplevel;
  GtkScrolledWindow *scroller;
  GtkSourceView *source_view;
  GtkHeaderBar *header_bar;
  IdeSourceSnippet *snippet;
  const gchar *text;
  GtkTextBuffer *buffer;

  g_assert (GB_IS_EDITOR_SETTINGS_WIDGET (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (row));
  snippet = g_object_get_data (G_OBJECT (row), "SNIPPET");

  window = g_object_new (GTK_TYPE_WINDOW,
                         "default-width", 600,
                         "default-height", 400,
                         "transient-for", toplevel,
                         "type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
                         NULL);

  header_bar = g_object_new (GTK_TYPE_HEADER_BAR,
                             "show-close-button", TRUE,
                             "visible", TRUE,
                             NULL);
  g_object_bind_property_full (snippet, "trigger",
                              header_bar, "title",
                              G_BINDING_SYNC_CREATE,
                              transform_title_func,
                              NULL, NULL, NULL);
  gtk_window_set_titlebar (window, GTK_WIDGET (header_bar));

  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "shadow-type", GTK_SHADOW_NONE,
                           "visible", TRUE,
                           NULL);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (scroller));

  source_view = g_object_new (GTK_SOURCE_TYPE_VIEW,
                              "editable", FALSE,
                              "monospace", TRUE,
                              "show-line-numbers", TRUE,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (source_view));

  text = ide_source_snippet_get_snippet_text (snippet);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  gtk_text_buffer_set_text (buffer, text, -1);

  gtk_window_present (window);
}

static void
gb_editor_settings_widget_finalize (GObject *object)
{
  GbEditorSettingsWidget *self = GB_EDITOR_SETTINGS_WIDGET (object);

  g_clear_pointer (&self->language, g_free);
  g_clear_object (&self->settings);

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

  object_class->finalize = gb_editor_settings_widget_finalize;
  object_class->get_property = gb_editor_settings_widget_get_property;
  object_class->set_property = gb_editor_settings_widget_set_property;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-settings-widget.ui");
  GB_WIDGET_CLASS_BIND (klass, GbEditorSettingsWidget, auto_indent);
  GB_WIDGET_CLASS_BIND (klass, GbEditorSettingsWidget, insert_matching_brace);
  GB_WIDGET_CLASS_BIND (klass, GbEditorSettingsWidget, insert_spaces_instead_of_tabs);
  GB_WIDGET_CLASS_BIND (klass, GbEditorSettingsWidget, right_margin_position);
  GB_WIDGET_CLASS_BIND (klass, GbEditorSettingsWidget, overwrite_braces);
  GB_WIDGET_CLASS_BIND (klass, GbEditorSettingsWidget, show_right_margin);
  GB_WIDGET_CLASS_BIND (klass, GbEditorSettingsWidget, snippets);
  GB_WIDGET_CLASS_BIND (klass, GbEditorSettingsWidget, snippets_container);
  GB_WIDGET_CLASS_BIND (klass, GbEditorSettingsWidget, tab_width);
  GB_WIDGET_CLASS_BIND (klass, GbEditorSettingsWidget, trim_trailing_whitespace);

  gParamSpecs [PROP_LANGUAGE] =
    g_param_spec_string ("language",
                         _("Language"),
                         _("The language to change the settings for."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_editor_settings_widget_init (GbEditorSettingsWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->snippets,
                           "row-activated",
                           G_CALLBACK (snippet_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
}
