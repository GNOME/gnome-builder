/* ide-layout-tab-private.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_LAYOUT_TAB_PRIVATE_H
#define IDE_LAYOUT_TAB_PRIVATE_H

#include <gtk/gtk.h>

#include "ide-layout-view.h"

G_BEGIN_DECLS

struct _IdeLayoutTab
{
  GtkEventBox    parent_instance;

  IdeLayoutView *view;
  GBinding      *modified_binding;
  GBinding      *title_binding;

  GtkWidget     *backward_button;
  GtkWidget     *controls_container;
  GtkWidget     *close_button;
  GtkWidget     *forward_button;
  GtkWidget     *modified_label;
  GtkWidget     *title_menu_button;
  GtkWidget     *title_label;
};

G_END_DECLS

#endif /* IDE_LAYOUT_TAB_PRIVATE_H */
