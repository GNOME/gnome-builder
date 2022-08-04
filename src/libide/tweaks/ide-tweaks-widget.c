/* ide-tweaks-widget.c
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

#define G_LOG_DOMAIN "ide-tweaks-widget"

#include "config.h"

#include "ide-tweaks-widget-private.h"

G_DEFINE_ABSTRACT_TYPE (IdeTweaksWidget, ide_tweaks_widget, IDE_TYPE_TWEAKS_ITEM)

static void
ide_tweaks_widget_class_init (IdeTweaksWidgetClass *klass)
{
}

static void
ide_tweaks_widget_init (IdeTweaksWidget *self)
{
}

GtkWidget *
_ide_tweaks_widget_create (IdeTweaksWidget *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_WIDGET (self), NULL);

  return IDE_TWEAKS_WIDGET_GET_CLASS (self)->create (self);
}
