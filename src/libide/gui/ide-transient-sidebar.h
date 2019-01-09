/* ide-transient-sidebar.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-panel.h"
#include "ide-page.h"

G_BEGIN_DECLS

#define IDE_TYPE_TRANSIENT_SIDEBAR (ide_transient_sidebar_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_DERIVABLE_TYPE (IdeTransientSidebar, ide_transient_sidebar, IDE, TRANSIENT_SIDEBAR, IdePanel)

struct _IdeTransientSidebarClass
{
  IdePanelClass parent_class;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_3_32
void ide_transient_sidebar_set_panel (IdeTransientSidebar *self,
                                      GtkWidget           *panel);
IDE_AVAILABLE_IN_3_32
void ide_transient_sidebar_set_page  (IdeTransientSidebar *self,
                                      IdePage             *page);
IDE_AVAILABLE_IN_3_32
void ide_transient_sidebar_lock      (IdeTransientSidebar *self);
IDE_AVAILABLE_IN_3_32
void ide_transient_sidebar_unlock    (IdeTransientSidebar *self);

G_END_DECLS
