/* gb-preferences-page-editor.c
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

#define G_LOG_DOMAIN "prefs-page-editor"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "gb-preferences-page-editor.h"
#include "gb-widget.h"

struct _GbPreferencesPageEditorPrivate
{
  GSettings                         *settings;

  /* Widgets owned by Template */
  GtkSwitch                         *restore_insert_mark_switch;
  GtkSwitch                         *show_diff_switch;
  GtkSwitch                         *word_completion_switch;
  GtkSwitch                         *show_line_numbers_switch;
  GtkSwitch                         *highlight_current_line_switch;
  GtkSwitch                         *highlight_matching_brackets_switch;
  GtkSwitch                         *smart_home_end_switch;
  GtkSwitch                         *show_grid_lines_switch;
  GtkFontButton                     *font_button;
  GtkSourceStyleSchemeChooserButton *style_scheme_button;

  /* Template widgets used for filtering */
  GtkWidget                         *restore_insert_mark_container;
  GtkWidget                         *word_completion_container;
  GtkWidget                         *show_diff_container;
  GtkWidget                         *show_line_numbers_container;
  GtkWidget                         *highlight_current_line_container;
  GtkWidget                         *highlight_matching_brackets_container;
  GtkWidget                         *smart_home_end_container;
  GtkWidget                         *show_grid_lines_container;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesPageEditor, gb_preferences_page_editor,
                            GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_editor_style_scheme_changed (GtkSourceStyleSchemeChooser *chooser,
                                                 GParamSpec                  *pspec,
                                                 GSettings                   *settings)
{
  GtkSourceStyleScheme *scheme;
  const gchar *scheme_id;

  g_return_if_fail (GTK_SOURCE_IS_STYLE_SCHEME_CHOOSER (chooser));
  g_return_if_fail (G_IS_SETTINGS (settings));

  scheme = gtk_source_style_scheme_chooser_get_style_scheme (chooser);

  if (scheme)
    {
      scheme_id = gtk_source_style_scheme_get_id (scheme);
      g_settings_set_string (settings, "style-scheme-name", scheme_id);
    }
}

static void
gb_preferences_page_editor_constructed (GObject *object)
{
  GbPreferencesPageEditorPrivate *priv;
  GbPreferencesPageEditor *editor = (GbPreferencesPageEditor *)object;
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme;
  gchar *scheme_id;

  g_return_if_fail (GB_IS_PREFERENCES_PAGE_EDITOR (editor));

  priv = editor->priv;

  priv->settings = g_settings_new ("org.gnome.builder.editor");

  g_settings_bind (priv->settings, "restore-insert-mark",
                   priv->restore_insert_mark_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "show-diff",
                   priv->show_diff_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "word-completion",
                   priv->word_completion_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "show-line-numbers",
                   priv->show_line_numbers_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "highlight-current-line",
                   priv->highlight_current_line_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "highlight-matching-brackets",
                   priv->highlight_matching_brackets_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "smart-home-end",
                   priv->smart_home_end_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "show-grid-lines",
                   priv->show_grid_lines_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->settings, "font-name",
                   priv->font_button, "font-name",
                   G_SETTINGS_BIND_DEFAULT);

  scheme_id = g_settings_get_string (priv->settings, "style-scheme-name");
  manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_id);
  g_free (scheme_id);
  gtk_source_style_scheme_chooser_set_style_scheme (
      GTK_SOURCE_STYLE_SCHEME_CHOOSER (priv->style_scheme_button),
      scheme);
  g_signal_connect_object (priv->style_scheme_button,
                           "notify::style-scheme",
                           G_CALLBACK (gb_preferences_page_editor_style_scheme_changed),
                           priv->settings,
                           0);

  G_OBJECT_CLASS (gb_preferences_page_editor_parent_class)->constructed (object);
}

static void
gb_preferences_page_editor_finalize (GObject *object)
{
  GbPreferencesPageEditorPrivate *priv = GB_PREFERENCES_PAGE_EDITOR (object)->priv;

  g_clear_object (&priv->settings);

  G_OBJECT_CLASS (gb_preferences_page_editor_parent_class)->finalize (object);
}

static void
gb_preferences_page_editor_class_init (GbPreferencesPageEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_preferences_page_editor_constructed;
  object_class->finalize = gb_preferences_page_editor_finalize;

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-preferences-page-editor.ui");

  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, font_button);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, restore_insert_mark_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_diff_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, style_scheme_button);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, word_completion_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_line_numbers_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, highlight_current_line_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, highlight_matching_brackets_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, smart_home_end_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_grid_lines_switch);

  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, restore_insert_mark_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, word_completion_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_diff_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_line_numbers_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, highlight_current_line_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, highlight_matching_brackets_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, smart_home_end_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_grid_lines_container);
}

static void
gb_preferences_page_editor_init (GbPreferencesPageEditor *self)
{
  self->priv = gb_preferences_page_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("restore insert cursor mark"),
                                               self->priv->restore_insert_mark_container,
                                               self->priv->restore_insert_mark_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("word words auto completion suggest found document"),
                                               self->priv->word_completion_container,
                                               self->priv->word_completion_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("diff renderer gutter changes git vcs"),
                                               self->priv->show_diff_container,
                                               self->priv->show_diff_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("line numbers"),
                                               self->priv->show_line_numbers_container,
                                               self->priv->show_line_numbers_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("line lines highlight current"),
                                               self->priv->highlight_current_line_container,
                                               self->priv->highlight_current_line_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("bracket brackets highlight matching"),
                                               self->priv->highlight_matching_brackets_container,
                                               self->priv->highlight_matching_brackets_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("smart home end"),
                                               self->priv->smart_home_end_container,
                                               self->priv->smart_home_end_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("show grid lines"),
                                               self->priv->show_grid_lines_container,
                                               self->priv->show_grid_lines_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("font document editor monospace"),
                                               GTK_WIDGET (self->priv->font_button),
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("source style scheme source tango solarized builder"),
                                               GTK_WIDGET (self->priv->style_scheme_button),
                                               NULL);
}
