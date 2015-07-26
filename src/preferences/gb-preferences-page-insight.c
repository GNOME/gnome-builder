/* gb-preferences-page-insight.c
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

#include "gb-preferences-page-insight.h"
#include "gb-preferences-switch.h"
#include "gb-widget.h"

struct _GbPreferencesPageInsight
{
  GbPreferencesPage    parent_instance;

  GbPreferencesSwitch *word_autocompletion;
  GbPreferencesSwitch *ctags_autocompletion;
  GbPreferencesSwitch *clang_autocompletion;
};

G_DEFINE_TYPE (GbPreferencesPageInsight, gb_preferences_page_insight, GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_insight_class_init (GbPreferencesPageInsightClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-preferences-page-insight.ui");
  gtk_widget_class_bind_template_child (widget_class, GbPreferencesPageInsight, ctags_autocompletion);
  gtk_widget_class_bind_template_child (widget_class, GbPreferencesPageInsight, clang_autocompletion);
  gtk_widget_class_bind_template_child (widget_class, GbPreferencesPageInsight, word_autocompletion);
}

static void
gb_preferences_page_insight_init (GbPreferencesPageInsight *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("word words auto completion suggest found document"),
                                               self->word_autocompletion,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("experimental clang autocompletion auto complete"),
                                               self->clang_autocompletion,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
  /* To translators: This is a list of keywords for the preferences page */
                                               _("exuberant ctags tags autocompletion auto complete"),
                                               self->ctags_autocompletion,
                                               NULL);
}
