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

  GtkWidget         *clang_autocompletion;
  GtkWidget         *ctags_autocompletion;
};

G_DEFINE_TYPE (GbPreferencesPageExperimental,
               gb_preferences_page_experimental,
               GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_experimental_class_init (GbPreferencesPageExperimentalClass *klass)
{
  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-preferences-page-experimental.ui");
  GB_WIDGET_CLASS_BIND (klass, GbPreferencesPageExperimental, clang_autocompletion);
  GB_WIDGET_CLASS_BIND (klass, GbPreferencesPageExperimental, ctags_autocompletion);
}

static void
gb_preferences_page_experimental_init (GbPreferencesPageExperimental *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("experimental clang autocompletion auto complete"),
                                               self->clang_autocompletion,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("experimental exhuberant ctags tags autocompletion auto complete"),
                                               self->ctags_autocompletion,
                                               NULL);
}
