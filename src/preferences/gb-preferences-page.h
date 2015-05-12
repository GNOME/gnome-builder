/* gb-preferences-page.h
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

#ifndef GB_PREFERENCES_PAGE_H
#define GB_PREFERENCES_PAGE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_PREFERENCES_PAGE (gb_preferences_page_get_type())

G_DECLARE_DERIVABLE_TYPE (GbPreferencesPage, gb_preferences_page, GB, PREFERENCES_PAGE, GtkBin)

struct _GbPreferencesPageClass
{
  GtkBinClass parent;

  /**
   * GbPreferencesPage::clear_search:
   *
   * Signal to let each preferences page clear
   * its local search when a global search is
   * about to be started
   */
  void (*clear_search) (GbPreferencesPage *self);
};

guint                  gb_preferences_page_set_keywords            (GbPreferencesPage   *page,
                                                                    const gchar * const *keywords);
void                   gb_preferences_page_set_keywords_for_widget (GbPreferencesPage   *page,
                                                                    const gchar         *keywords,
                                                                    gpointer             first_widget,
                                                                    ...) G_GNUC_NULL_TERMINATED;
void                   gb_preferences_page_set_title               (GbPreferencesPage *page,
                                                                    const gchar       *title);
void                   gb_preferences_page_reset_title             (GbPreferencesPage *page);
GtkWidget             *gb_preferences_page_get_controls            (GbPreferencesPage *page);
void                   gb_preferences_page_clear_search            (GbPreferencesPage *page);

G_END_DECLS

#endif /* GB_PREFERENCES_PAGE_H */
