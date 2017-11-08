/* ide-gtk.h
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#include "ide-context.h"

#include "workbench/ide-workbench.h"

G_BEGIN_DECLS

typedef void (*IdeWidgetContextHandler) (GtkWidget  *widget,
                                         IdeContext *context);

IDE_AVAILABLE_IN_ALL
void          ide_widget_set_context_handler (gpointer                 widget,
                                              IdeWidgetContextHandler  handler);
IDE_AVAILABLE_IN_ALL
IdeContext   *ide_widget_get_context         (GtkWidget               *widget);
IDE_AVAILABLE_IN_ALL
IdeWorkbench *ide_widget_get_workbench       (GtkWidget               *widget);

G_END_DECLS
