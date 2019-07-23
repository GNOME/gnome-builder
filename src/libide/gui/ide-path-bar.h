/* ide-path-bar.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>
#include <libide-core.h>

#include "ide-path.h"

G_BEGIN_DECLS

#define IDE_TYPE_PATH_BAR (ide_path_bar_get_type())

IDE_AVAILABLE_IN_3_34
G_DECLARE_DERIVABLE_TYPE (IdePathBar, ide_path_bar, IDE, PATH_BAR, GtkContainer)

struct _IdePathBarClass
{
  GtkContainerClass parent_instance;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_3_34
GtkWidget *ide_path_bar_new           (void);
IDE_AVAILABLE_IN_3_34
IdePath   *ide_path_bar_get_path      (IdePathBar *self);
IDE_AVAILABLE_IN_3_34
void       ide_path_bar_set_path      (IdePathBar *path_bar,
                                       IdePath    *path);
IDE_AVAILABLE_IN_3_34
IdePath   *ide_path_bar_get_selection (IdePathBar *self);

G_END_DECLS
