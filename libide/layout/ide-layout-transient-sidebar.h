/* ide-layout-transient-sidebar.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>

#include "layout/ide-layout-pane.h"
#include "layout/ide-layout-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_LAYOUT_TRANSIENT_SIDEBAR (ide_layout_transient_sidebar_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeLayoutTransientSidebar, ide_layout_transient_sidebar, IDE, LAYOUT_TRANSIENT_SIDEBAR, IdeLayoutPane)

struct _IdeLayoutTransientSidebarClass
{
  IdeLayoutPaneClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

void ide_layout_transient_sidebar_set_view (IdeLayoutTransientSidebar *self,
                                            IdeLayoutView             *view);

G_END_DECLS
