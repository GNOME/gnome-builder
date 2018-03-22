/* ide-workbench-header-bar.h
 *
 * Copyright 2015 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gtk/gtk.h>

#include "ide-version-macros.h"

#include "workbench/ide-omni-bar.h"

G_BEGIN_DECLS

#define IDE_TYPE_WORKBENCH_HEADER_BAR (ide_workbench_header_bar_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeWorkbenchHeaderBar, ide_workbench_header_bar, IDE, WORKBENCH_HEADER_BAR, GtkHeaderBar)

struct _IdeWorkbenchHeaderBarClass
{
  GtkHeaderBarClass parent;

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IDE_AVAILABLE_IN_ALL
GtkWidget  *ide_workbench_header_bar_new          (void);
IDE_AVAILABLE_IN_ALL
IdeOmniBar *ide_workbench_header_bar_get_omni_bar (IdeWorkbenchHeaderBar *self);
IDE_AVAILABLE_IN_ALL
void        ide_workbench_header_bar_focus_search (IdeWorkbenchHeaderBar *self);
IDE_AVAILABLE_IN_ALL
void        ide_workbench_header_bar_add_primary  (IdeWorkbenchHeaderBar *self,
                                                   GtkWidget             *widget);
IDE_AVAILABLE_IN_ALL
void        ide_workbench_header_bar_insert_left  (IdeWorkbenchHeaderBar *self,
                                                   GtkWidget             *widget,
                                                   GtkPackType            pack_type,
                                                   gint                   priority);
IDE_AVAILABLE_IN_ALL
void        ide_workbench_header_bar_insert_right (IdeWorkbenchHeaderBar *self,
                                                   GtkWidget             *widget,
                                                   GtkPackType            pack_type,
                                                   gint                   priority);

G_END_DECLS
