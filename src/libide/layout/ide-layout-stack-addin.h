/* ide-layout-stack-addin.h
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include "layout/ide-layout-stack.h"
#include "layout/ide-layout-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_LAYOUT_STACK_ADDIN (ide_layout_stack_addin_get_type())

G_DECLARE_INTERFACE (IdeLayoutStackAddin, ide_layout_stack_addin, IDE, LAYOUT_STACK_ADDIN, GObject)

struct _IdeLayoutStackAddinInterface
{
  GTypeInterface parent_iface;

  void (*load)     (IdeLayoutStackAddin *self,
                    IdeLayoutStack      *stack);
  void (*unload)   (IdeLayoutStackAddin *self,
                    IdeLayoutStack      *stack);
  void (*set_view) (IdeLayoutStackAddin *self,
                    IdeLayoutView       *view);
};

IDE_AVAILABLE_IN_ALL
void                 ide_layout_stack_addin_load                (IdeLayoutStackAddin *self,
                                                                 IdeLayoutStack      *stack);
IDE_AVAILABLE_IN_ALL
void                 ide_layout_stack_addin_unload              (IdeLayoutStackAddin *self,
                                                                 IdeLayoutStack      *stack);
IDE_AVAILABLE_IN_ALL
void                 ide_layout_stack_addin_set_view            (IdeLayoutStackAddin *self,
                                                                 IdeLayoutView       *view);
IDE_AVAILABLE_IN_ALL
IdeLayoutStackAddin *ide_layout_stack_addin_find_by_module_name (IdeLayoutStack      *stack,
                                                                 const gchar         *module_name);

G_END_DECLS
