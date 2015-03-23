/* gb-preferences-page-experimental.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "gb-preferences-page-experimental.h"
#include "gb-widget.h"

struct _GbPreferencesPageExperimental
{
  GbPreferencesPage  parent_instance;

  GtkWidget         *clang_autocompletion_container;
  GtkWidget         *clang_autocompletion_switch;
};

G_DEFINE_TYPE (GbPreferencesPageExperimental,
               gb_preferences_page_experimental,
               GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_experimental_class_init (GbPreferencesPageExperimentalClass *klass)
{
  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-preferences-page-experimental.ui");
  GB_WIDGET_CLASS_BIND (klass, GbPreferencesPageExperimental, clang_autocompletion_container);
  GB_WIDGET_CLASS_BIND (klass, GbPreferencesPageExperimental, clang_autocompletion_switch);
}

static void
gb_preferences_page_experimental_init (GbPreferencesPageExperimental *self)
{
  g_autoptr(GSettings) settings = NULL;
  GAction *action;
  GSimpleActionGroup *group;

  gtk_widget_init_template (GTK_WIDGET (self));

  settings = g_settings_new ("org.gnome.builder.experimental");

  group = g_simple_action_group_new ();

  action = g_settings_create_action (settings, "clang-autocompletion");
  g_action_map_add_action (G_ACTION_MAP (group), action);
  g_clear_object (&action);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "settings", G_ACTION_GROUP (group));

  /* To translators: This is a list of keywords for the preferences page */
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("experimental clang autocompletion auto complete"),
                                               self->clang_autocompletion_container,
                                               NULL);
}
