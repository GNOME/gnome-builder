/* ide-surface.h
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

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <dazzle.h>
#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_SURFACE (ide_surface_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_DERIVABLE_TYPE (IdeSurface, ide_surface, IDE, SURFACE, DzlDockBin)

struct _IdeSurfaceClass
{
  DzlDockBinClass parent_class;

  void     (*foreach_page)        (IdeSurface  *self,
                                   GtkCallback  callback,
                                   gpointer     user_data);
  gboolean (*agree_to_shutdown)   (IdeSurface  *self);
  void     (*set_fullscreen)      (IdeSurface  *self,
                                   gboolean     fullscreen);

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_3_32
GtkWidget *ide_surface_new               (void);
IDE_AVAILABLE_IN_3_32
void       ide_surface_set_icon_name     (IdeSurface  *self,
                                          const gchar *icon_name);
IDE_AVAILABLE_IN_3_32
void       ide_surface_set_title         (IdeSurface  *self,
                                          const gchar *title);
IDE_AVAILABLE_IN_3_32
void       ide_surface_foreach_page      (IdeSurface  *self,
                                          GtkCallback  callback,
                                          gpointer     user_data);
IDE_AVAILABLE_IN_3_32
gboolean   ide_surface_agree_to_shutdown (IdeSurface  *self);

G_END_DECLS
