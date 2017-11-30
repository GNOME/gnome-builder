/* ide-workbench-private.h
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

#ifndef IDE_WORKBENCH_PRIVATE_H
#define IDE_WORKBENCH_PRIVATE_H

#include <libpeas/peas.h>

#include "workbench/ide-perspective.h"
#include "workbench/ide-workbench.h"
#include "workbench/ide-workbench-header-bar.h"

G_BEGIN_DECLS

struct _IdeWorkbench
{
  DzlApplicationWindow       parent;

  guint                      unloading : 1;
  guint                      focus_mode : 1;
  guint                      disable_greeter : 1;
  guint                      early_perspectives_removed : 1;
  guint                      did_initial_editor_transition : 1;

  IdeContext                *context;
  GCancellable              *cancellable;
  PeasExtensionSet          *addins;

  GtkStack                  *header_stack;
  IdeWorkbenchHeaderBar     *header_bar;
  DzlMenuButton             *perspective_menu_button;
  GtkStack                  *perspectives_stack;
  GtkSizeGroup              *header_size_group;
  GtkBox                    *message_box;

  GObject                   *selection_owner;
};

void     ide_workbench_set_context                (IdeWorkbench          *workbench,
                                                   IdeContext            *context);
void     ide_workbench_actions_init               (IdeWorkbench          *self);
void     ide_workbench_set_selection_owner        (IdeWorkbench          *self,
                                                   GObject               *object);
GObject *ide_workbench_get_selection_owner        (IdeWorkbench          *self);

void     _ide_workbench_header_bar_set_fullscreen (IdeWorkbenchHeaderBar *self,
                                                   gboolean               fullscreen);
void     _ide_workbench_add_perspective_shortcut  (IdeWorkbench          *self,
                                                   IdePerspective        *perspective);
void     _ide_workbench_init_shortcuts            (IdeWorkbench          *self);

G_END_DECLS

#endif /* IDE_WORKBENCH_PRIVATE_H */
