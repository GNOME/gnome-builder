/* gb-editor-navigation-item.h
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

#ifndef GB_EDITOR_NAVIGATION_ITEM_H
#define GB_EDITOR_NAVIGATION_ITEM_H

#include <gio/gio.h>

#include "gb-navigation-item.h"

G_BEGIN_DECLS

#define GB_TYPE_EDITOR_NAVIGATION_ITEM            (gb_editor_navigation_item_get_type())
#define GB_EDITOR_NAVIGATION_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_NAVIGATION_ITEM, GbEditorNavigationItem))
#define GB_EDITOR_NAVIGATION_ITEM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_NAVIGATION_ITEM, GbEditorNavigationItem const))
#define GB_EDITOR_NAVIGATION_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_EDITOR_NAVIGATION_ITEM, GbEditorNavigationItemClass))
#define GB_IS_EDITOR_NAVIGATION_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_EDITOR_NAVIGATION_ITEM))
#define GB_IS_EDITOR_NAVIGATION_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_EDITOR_NAVIGATION_ITEM))
#define GB_EDITOR_NAVIGATION_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_EDITOR_NAVIGATION_ITEM, GbEditorNavigationItemClass))

typedef struct _GbEditorNavigationItem        GbEditorNavigationItem;
typedef struct _GbEditorNavigationItemClass   GbEditorNavigationItemClass;
typedef struct _GbEditorNavigationItemPrivate GbEditorNavigationItemPrivate;

struct _GbEditorNavigationItem
{
  GbNavigationItem parent;

  /*< private >*/
  GbEditorNavigationItemPrivate *priv;
};

struct _GbEditorNavigationItemClass
{
  GbNavigationItemClass parent;
};

GFile                *gb_editor_navigation_item_get_file        (GbEditorNavigationItem *item);
guint                 gb_editor_navigation_item_get_line        (GbEditorNavigationItem *item);
guint                 gb_editor_navigation_item_get_line_offset (GbEditorNavigationItem *item);
GType                 gb_editor_navigation_item_get_type        (void) G_GNUC_CONST;
GbNavigationItem     *gb_editor_navigation_item_new             (GFile                  *file,
                                                                 guint                   line,
                                                                 guint                   line_offset);

G_END_DECLS

#endif /* GB_EDITOR_NAVIGATION_ITEM_H */
