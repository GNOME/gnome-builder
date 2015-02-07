/* gb-preferences-page-emacs.h
 *
 * Copyright (C) 2015 Roberto Majadas <roberto.majadas@openshine.com>
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

#ifndef GB_PREFERENCES_PAGE_EMACS_H
#define GB_PREFERENCES_PAGE_EMACS_H

#include "gb-preferences-page.h"

G_BEGIN_DECLS

#define GB_TYPE_PREFERENCES_PAGE_EMACS            (gb_preferences_page_emacs_get_type())
#define GB_PREFERENCES_PAGE_EMACS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_PREFERENCES_PAGE_EMACS, GbPreferencesPageEmacs))
#define GB_PREFERENCES_PAGE_EMACS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_PREFERENCES_PAGE_EMACS, GbPreferencesPageEmacs const))
#define GB_PREFERENCES_PAGE_EMACS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_PREFERENCES_PAGE_EMACS, GbPreferencesPageEmacsClass))
#define GB_IS_PREFERENCES_PAGE_EMACS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_PREFERENCES_PAGE_EMACS))
#define GB_IS_PREFERENCES_PAGE_EMACS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_PREFERENCES_PAGE_EMACS))
#define GB_PREFERENCES_PAGE_EMACS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_PREFERENCES_PAGE_EMACS, GbPreferencesPageEmacsClass))

typedef struct _GbPreferencesPageEmacs        GbPreferencesPageEmacs;
typedef struct _GbPreferencesPageEmacsClass   GbPreferencesPageEmacsClass;
typedef struct _GbPreferencesPageEmacsPrivate GbPreferencesPageEmacsPrivate;

struct _GbPreferencesPageEmacs
{
  GbPreferencesPage parent;

  /*< private >*/
  GbPreferencesPageEmacsPrivate *priv;
};

struct _GbPreferencesPageEmacsClass
{
  GbPreferencesPageClass parent;
};

GType                     gb_preferences_page_emacs_get_type (void);

G_END_DECLS

#endif /* GB_PREFERENCES_PAGE_EMACS_H */