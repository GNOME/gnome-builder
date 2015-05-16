/* gb-preferences-page-theme.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "gb-preferences-page-theme.h"
#include "gb-widget.h"

struct _GbPreferencesPageTheme
{
  GbPreferencesPage                  parent_instance;

  GSettings                         *editor_settings;

  GtkSourceStyleSchemeChooserWidget *style_scheme_widget;
  GtkSwitch                         *show_grid_lines_switch;
};

G_DEFINE_TYPE (GbPreferencesPageTheme, gb_preferences_page_theme, GB_TYPE_PREFERENCES_PAGE)

static void
style_scheme_changed (GtkSourceStyleSchemeChooser *chooser,
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
gb_preferences_page_theme_constructed (GObject *object)
{
  GbPreferencesPageTheme *self = (GbPreferencesPageTheme *)object;
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme;
  gchar *scheme_id;

  G_OBJECT_CLASS (gb_preferences_page_theme_parent_class)->constructed (object);

  scheme_id = g_settings_get_string (self->editor_settings, "style-scheme-name");
  manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_id);
  g_free (scheme_id);

  gtk_source_style_scheme_chooser_set_style_scheme (
      GTK_SOURCE_STYLE_SCHEME_CHOOSER (self->style_scheme_widget),
      scheme);
  g_signal_connect_object (self->style_scheme_widget,
                           "notify::style-scheme",
                           G_CALLBACK (style_scheme_changed),
                           self->editor_settings,
                           0);
}

static void
gb_preferences_page_theme_finalize (GObject *object)
{
  GbPreferencesPageTheme *self = (GbPreferencesPageTheme *)object;

  g_clear_object (&self->editor_settings);

  G_OBJECT_CLASS (gb_preferences_page_theme_parent_class)->finalize (object);
}

static void
gb_preferences_page_theme_class_init (GbPreferencesPageThemeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_preferences_page_theme_constructed;
  object_class->finalize = gb_preferences_page_theme_finalize;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-preferences-page-theme.ui");
  GB_WIDGET_CLASS_BIND (klass, GbPreferencesPageTheme, show_grid_lines_switch);
  GB_WIDGET_CLASS_BIND (klass, GbPreferencesPageTheme, style_scheme_widget);
}

static void
gb_preferences_page_theme_init (GbPreferencesPageTheme *self)
{
  self->editor_settings = g_settings_new ("org.gnome.builder.editor");

  gtk_widget_init_template (GTK_WIDGET (self));

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("show grid lines"),
                                               self->show_grid_lines_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("source style scheme source tango solarized builder syntax"),
                                               self->style_scheme_widget,
                                               NULL);
}
