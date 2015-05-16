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

struct _GbPreferencesPageEditor
{
  GbPreferencesPage                  parent_instance;

  GSettings                         *editor_settings;
  GtkSwitch                         *restore_insert_mark_switch;
  GtkSwitch                         *show_diff_switch;
  GtkSwitch                         *show_line_numbers_switch;
  GtkSwitch                         *highlight_current_line_switch;
  GtkSwitch                         *highlight_matching_brackets_switch;
  GtkSwitch                         *show_grid_lines_switch;
  GtkSpinButton                     *scroll_off_spin;
  GtkFontButton                     *font_button;
  GtkSourceStyleSchemeChooserWidget *style_scheme_widget;
  GtkScrolledWindow                 *style_scheme_container;
  GtkAdjustment                     *scroll_off_adjustment;
  GtkBox                            *scroll_off_container;
  GtkWidget                         *auto_hide_map_switch;
  GtkWidget                         *show_map_switch;
};

G_DEFINE_TYPE (GbPreferencesPageEditor, gb_preferences_page_editor, GB_TYPE_PREFERENCES_PAGE)

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
  GbPreferencesPageEditor *self = (GbPreferencesPageEditor *)object;
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme;
  gchar *scheme_id;

  g_assert (GB_IS_PREFERENCES_PAGE_EDITOR (self));

  G_OBJECT_CLASS (gb_preferences_page_editor_parent_class)->constructed (object);

  g_settings_bind (self->editor_settings, "scroll-offset",
                   self->scroll_off_adjustment, "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->editor_settings, "font-name",
                   self->font_button, "font-name",
                   G_SETTINGS_BIND_DEFAULT);

  scheme_id = g_settings_get_string (self->editor_settings, "style-scheme-name");
  manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_id);
  g_free (scheme_id);

  gtk_source_style_scheme_chooser_set_style_scheme (
      GTK_SOURCE_STYLE_SCHEME_CHOOSER (self->style_scheme_widget),
      scheme);
  g_signal_connect_object (self->style_scheme_widget,
                           "notify::style-scheme",
                           G_CALLBACK (gb_preferences_page_editor_style_scheme_changed),
                           self->editor_settings,
                           0);
}

static void
gb_preferences_page_editor_class_init (GbPreferencesPageEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_preferences_page_editor_constructed;

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-preferences-page-editor.ui");

  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_map_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, editor_settings);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, font_button);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, highlight_current_line_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, highlight_matching_brackets_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, auto_hide_map_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, restore_insert_mark_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, scroll_off_adjustment);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, scroll_off_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, scroll_off_spin);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_diff_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_grid_lines_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, show_line_numbers_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, style_scheme_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageEditor, style_scheme_widget);
}

static void
gb_preferences_page_editor_init (GbPreferencesPageEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("restore insert cursor mark"),
                                               self->restore_insert_mark_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("diff renderer gutter changes git vcs"),
                                               self->show_diff_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("line numbers"),
                                               self->show_line_numbers_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("line lines highlight current"),
                                               self->highlight_current_line_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("bracket brackets highlight matching"),
                                               self->highlight_matching_brackets_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("show grid lines"),
                                               self->show_grid_lines_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("lines margin scrolloff scroll off"),
                                               self->scroll_off_container,
                                               self->scroll_off_spin,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("font document editor monospace"),
                                               self->font_button,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("source style scheme source tango solarized builder syntax"),
                                               self->style_scheme_container,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("minimap mini map overview over view"),
                                               self->show_map_switch,
                                               self->auto_hide_map_switch,
                                               NULL);
}
