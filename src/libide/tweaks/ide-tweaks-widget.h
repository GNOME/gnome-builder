/* ide-tweaks-widget.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_TWEAKS_INSIDE) && !defined (IDE_TWEAKS_COMPILATION)
# error "Only <libide-tweaks.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include "ide-tweaks-binding.h"
#include "ide-tweaks-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_WIDGET (ide_tweaks_widget_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeTweaksWidget, ide_tweaks_widget, IDE, TWEAKS_WIDGET, IdeTweaksItem)

struct _IdeTweaksWidgetClass
{
  IdeTweaksItemClass parent_class;

  GtkWidget *(*create_for_item) (IdeTweaksWidget *self,
                                 IdeTweaksItem   *item);
};

IDE_AVAILABLE_IN_ALL
IdeTweaksWidget  *ide_tweaks_widget_new (void);
IDE_AVAILABLE_IN_ALL
IdeTweaksBinding *ide_tweaks_widget_get_binding (IdeTweaksWidget  *self);
IDE_AVAILABLE_IN_ALL
void              ide_tweaks_widget_set_binding (IdeTweaksWidget  *self,
                                                 IdeTweaksBinding *binding);

G_END_DECLS
