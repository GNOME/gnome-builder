/* gb-command-bar-item.h
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

#ifndef GB_COMMAND_BAR_ITEM_H
#define GB_COMMAND_BAR_ITEM_H

#include <gtk/gtk.h>

#include "gb-command-result.h"

G_BEGIN_DECLS

#define GB_TYPE_COMMAND_BAR_ITEM            (gb_command_bar_item_get_type())
#define GB_COMMAND_BAR_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_COMMAND_BAR_ITEM, GbCommandBarItem))
#define GB_COMMAND_BAR_ITEM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_COMMAND_BAR_ITEM, GbCommandBarItem const))
#define GB_COMMAND_BAR_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_COMMAND_BAR_ITEM, GbCommandBarItemClass))
#define GB_IS_COMMAND_BAR_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_COMMAND_BAR_ITEM))
#define GB_IS_COMMAND_BAR_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_COMMAND_BAR_ITEM))
#define GB_COMMAND_BAR_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_COMMAND_BAR_ITEM, GbCommandBarItemClass))

typedef struct _GbCommandBarItem        GbCommandBarItem;
typedef struct _GbCommandBarItemClass   GbCommandBarItemClass;
typedef struct _GbCommandBarItemPrivate GbCommandBarItemPrivate;

struct _GbCommandBarItem
{
  GtkBin parent;

  /*< private >*/
  GbCommandBarItemPrivate *priv;
};

struct _GbCommandBarItemClass
{
  GtkBinClass parent;
};

GType      gb_command_bar_item_get_type   (void);
GtkWidget *gb_command_bar_item_new        (GbCommandResult  *result);
GtkWidget *gb_command_bar_item_get_result (GbCommandBarItem *item);

G_END_DECLS

#endif /* GB_COMMAND_BAR_ITEM_H */
