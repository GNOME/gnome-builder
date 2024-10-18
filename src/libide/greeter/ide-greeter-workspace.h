/* ide-greeter-workspace.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_GREETER_INSIDE) && !defined (IDE_GREETER_COMPILATION)
# error "Only <libide-greeter.h> can be included directly."
#endif

#include <libide-projects.h>
#include <libide-gui.h>

#include "ide-greeter-section.h"

G_BEGIN_DECLS

#define IDE_TYPE_GREETER_WORKSPACE (ide_greeter_workspace_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeGreeterWorkspace, ide_greeter_workspace, IDE, GREETER_WORKSPACE, IdeWorkspace)

IDE_AVAILABLE_IN_ALL
IdeGreeterWorkspace *ide_greeter_workspace_new                (IdeApplication      *app);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_add_section        (IdeGreeterWorkspace *self,
                                                               IdeGreeterSection   *section);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_remove_section     (IdeGreeterWorkspace *self,
                                                               IdeGreeterSection   *section);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_add_button         (IdeGreeterWorkspace *self,
                                                               GtkWidget           *button,
                                                               gint                 priority);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_begin              (IdeGreeterWorkspace *self);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_end                (IdeGreeterWorkspace *self);
IDE_AVAILABLE_IN_ALL
gboolean             ide_greeter_workspace_get_selection_mode (IdeGreeterWorkspace *self);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_set_selection_mode (IdeGreeterWorkspace *self,
                                                               gboolean             selection_mode);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_open_project       (IdeGreeterWorkspace *self,
                                                               IdeProjectInfo      *project_info);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_add_page           (IdeGreeterWorkspace *self,
                                                               AdwNavigationPage   *page);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_remove_page        (IdeGreeterWorkspace *self,
                                                               AdwNavigationPage   *page);
IDE_AVAILABLE_IN_ALL
AdwNavigationPage   *ide_greeter_workspace_get_visible_page   (IdeGreeterWorkspace *self);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_push_page          (IdeGreeterWorkspace *self,
                                                               AdwNavigationPage   *page);
IDE_AVAILABLE_IN_ALL
AdwNavigationPage   *ide_greeter_workspace_find_page          (IdeGreeterWorkspace *self,
                                                               const char          *tag);
IDE_AVAILABLE_IN_ALL
void                 ide_greeter_workspace_push_page_by_tag   (IdeGreeterWorkspace *self,
                                                               const char          *tag);
IDE_AVAILABLE_IN_48
gboolean             ide_greeter_workspace_is_busy            (IdeGreeterWorkspace *self);

G_END_DECLS
