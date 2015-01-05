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

#define GB_TYPE_PREFERENCES_PAGE            (gb_preferences_page_get_type())
#define GB_PREFERENCES_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_PREFERENCES_PAGE, GbPreferencesPage))
#define GB_PREFERENCES_PAGE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_PREFERENCES_PAGE, GbPreferencesPage const))
#define GB_PREFERENCES_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_PREFERENCES_PAGE, GbPreferencesPageClass))
#define GB_IS_PREFERENCES_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_PREFERENCES_PAGE))
#define GB_IS_PREFERENCES_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_PREFERENCES_PAGE))
#define GB_PREFERENCES_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_PREFERENCES_PAGE, GbPreferencesPageClass))

typedef struct _GbPreferencesPage        GbPreferencesPage;
typedef struct _GbPreferencesPageClass   GbPreferencesPageClass;
typedef struct _GbPreferencesPagePrivate GbPreferencesPagePrivate;

struct _GbPreferencesPage
{
  GtkBin parent;

  /*< private >*/
  GbPreferencesPagePrivate *priv;
};

struct _GbPreferencesPageClass
{
  GtkBinClass parent;
};

GType    gb_preferences_page_get_type                (void);
guint    gb_preferences_page_set_keywords            (GbPreferencesPage   *page,
                                                      const gchar * const *keywords);
void     gb_preferences_page_set_keywords_for_widget (GbPreferencesPage   *page,
                                                      const gchar         *keywords,
                                                      GtkWidget           *first_widget,
                                                      ...) G_GNUC_NULL_TERMINATED;
gboolean gb_preferences_page_get_active              (GbPreferencesPage   *page);
void     gb_preferences_page_set_active              (GbPreferencesPage   *page,
                                                      gboolean             active);

G_END_DECLS

#endif /* GB_PREFERENCES_PAGE_H */
