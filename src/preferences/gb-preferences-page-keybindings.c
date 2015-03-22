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

  GSettings             *editor_settings;

  /* Template widgets used for filtering */
  GtkWidget             *default_container;
  GtkWidget             *emacs_container;
  GtkWidget             *vim_container;
};

G_DEFINE_TYPE (GbPreferencesPageKeybindings,
               gb_preferences_page_keybindings,
               GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_keybindings_constructed (GObject *object)
{
  GbPreferencesPageKeybindings *self = (GbPreferencesPageKeybindings *)object;
  GSimpleActionGroup *group;
  GAction *action;

  g_return_if_fail (GB_IS_PREFERENCES_PAGE_KEYBINDINGS (self));

  self->editor_settings = g_settings_new ("org.gnome.builder.editor");

  group = g_simple_action_group_new ();

  action = g_settings_create_action (self->editor_settings, "keybindings");
  g_action_map_add_action (G_ACTION_MAP (group), action);
  g_clear_object (&action);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "settings", G_ACTION_GROUP (group));
  g_clear_object (&group);
}

static void
gb_preferences_page_keybindings_finalize (GObject *object)
{
  GbPreferencesPageKeybindings *self = (GbPreferencesPageKeybindings *)object;

  g_clear_object (&self->editor_settings);

  G_OBJECT_CLASS (gb_preferences_page_keybindings_parent_class)->finalize (object);
}

static void
gb_preferences_page_keybindings_class_init (GbPreferencesPageKeybindingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_preferences_page_keybindings_finalize;
  object_class->constructed = gb_preferences_page_keybindings_constructed;

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-preferences-page-keybindings.ui");
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageKeybindings, vim_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageKeybindings, emacs_container);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageKeybindings, default_container);
}

static void
gb_preferences_page_keybindings_init (GbPreferencesPageKeybindings *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* To translators: This is a list of keywords for the preferences page */
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("default keybindings"),
                                               self->default_container,
                                               NULL);
  /* To translators: This is a list of keywords for the preferences page */
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("emacs keybindings modal"),
                                               self->emacs_container,
                                               NULL);
  /* To translators: This is a list of keywords for the preferences page */
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("vim keybindings modal"),
                                               self->vim_container,
                                               NULL);
}
