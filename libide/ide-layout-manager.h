/* ide-layout-manager.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_LAYOUT_MANAGER_H
#define IDE_LAYOUT_MANAGER_H

#include <gtk/gtk.h>

#include "ide-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_LAYOUT_MANAGER (ide_layout_manager_get_type())

G_DECLARE_INTERFACE (IdeLayoutManager, ide_layout_manager, IDE, LAYOUT_MANAGER, GObject)

typedef struct
{
  GtkWidget *left_of;
  GtkWidget *right_of;
  GtkWidget *above;
  GtkWidget *below;
  gint       column;
} IdeLayoutHints;

struct _IdeLayoutManagerInterface
{
  GTypeInterface parent_instance;

  guint (*add)    (IdeLayoutManager     *self,
                   const IdeLayoutHints *hints,
                   GtkWidget            *child);
  void  (*remove) (IdeLayoutManager     *self,
                   guint                 layout_id);
};

guint ide_layout_manager_add    (IdeLayoutManager     *self,
                                 const IdeLayoutHints *hints,
                                 GtkWidget            *child);
void  ide_layout_manager_remove (IdeLayoutManager     *self,
                                 guint                 layout_id);

G_END_DECLS

#endif /* IDE_LAYOUT_MANAGER_H */
