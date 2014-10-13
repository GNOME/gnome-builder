/* gb-preferences-page-editor.h
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

#ifndef GB_PREFERENCES_PAGE_EDITOR_H
#define GB_PREFERENCES_PAGE_EDITOR_H

#include "gb-preferences-page.h"

G_BEGIN_DECLS

#define GB_TYPE_PREFERENCES_PAGE_EDITOR            (gb_preferences_page_editor_get_type())
#define GB_PREFERENCES_PAGE_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_PREFERENCES_PAGE_EDITOR, GbPreferencesPageEditor))
#define GB_PREFERENCES_PAGE_EDITOR_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_PREFERENCES_PAGE_EDITOR, GbPreferencesPageEditor const))
#define GB_PREFERENCES_PAGE_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_PREFERENCES_PAGE_EDITOR, GbPreferencesPageEditorClass))
#define GB_IS_PREFERENCES_PAGE_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_PREFERENCES_PAGE_EDITOR))
#define GB_IS_PREFERENCES_PAGE_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_PREFERENCES_PAGE_EDITOR))
#define GB_PREFERENCES_PAGE_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_PREFERENCES_PAGE_EDITOR, GbPreferencesPageEditorClass))

typedef struct _GbPreferencesPageEditor        GbPreferencesPageEditor;
typedef struct _GbPreferencesPageEditorClass   GbPreferencesPageEditorClass;
typedef struct _GbPreferencesPageEditorPrivate GbPreferencesPageEditorPrivate;

struct _GbPreferencesPageEditor
{
  GtkBin parent;

  /*< private >*/
  GbPreferencesPageEditorPrivate *priv;
};

struct _GbPreferencesPageEditorClass
{
  GtkBinClass parent;
};

GType gb_preferences_page_editor_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GB_PREFERENCES_PAGE_EDITOR_H */
