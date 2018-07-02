/* ide-hover-popover-private.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "ide-hover-context.h"
#include "ide-hover-provider.h"

G_BEGIN_DECLS

#define IDE_TYPE_HOVER_POPOVER (ide_hover_popover_get_type())

G_DECLARE_FINAL_TYPE (IdeHoverPopover, ide_hover_popover, IDE, HOVER_POPOVER, GtkPopover)

IdeHoverContext *_ide_hover_popover_get_context  (IdeHoverPopover  *self);
void             _ide_hover_popover_add_provider (IdeHoverPopover  *self,
                                                  IdeHoverProvider *provider);
void             _ide_hover_popover_show         (IdeHoverPopover  *self);
void             _ide_hover_popover_hide         (IdeHoverPopover  *self);

G_END_DECLS
