/* gb-preferences-page-keybindings.h
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
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

#define G_LOG_DOMAIN "gb-preferences-page-keybindings"

#include <glib/gi18n.h>

#include "gb-preferences-page-keybindings.h"
#include "gb-widget.h"

struct _GbPreferencesPageKeybindings
{
  GbPreferencesPage      parent_instance;

  /* Template widgets used for filtering */
  GtkWidget             *default_switch;
  GtkWidget             *emacs_switch;
  GtkWidget             *vim_switch;
  GtkSwitch             *smart_backspace_switch;
  GtkSwitch             *smart_home_end_switch;
};

G_DEFINE_TYPE (GbPreferencesPageKeybindings,
               gb_preferences_page_keybindings,
               GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_keybindings_class_init (GbPreferencesPageKeybindingsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-preferences-page-keybindings.ui");

  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageKeybindings, vim_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageKeybindings, emacs_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageKeybindings, default_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageKeybindings, smart_home_end_switch);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageKeybindings, smart_backspace_switch);
}

static void
gb_preferences_page_keybindings_init (GbPreferencesPageKeybindings *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("default builder keybindings"),
                                               self->default_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("emacs keybindings modal"),
                                               self->emacs_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("vim keybindings modal"),
                                               self->vim_switch,
                                               NULL);

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("smart home end"),
                                               self->smart_home_end_switch,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("smart back backspace indent align"),
                                               self->smart_backspace_switch,
                                               NULL);
}
