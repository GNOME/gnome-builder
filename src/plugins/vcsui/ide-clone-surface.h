/* ide-clone-surface.h
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

#include <libide-gui.h>

G_BEGIN_DECLS

#define IDE_TYPE_CLONE_SURFACE (ide_clone_surface_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeCloneSurface, ide_clone_surface, IDE, CLONE_SURFACE, IdeSurface)

IDE_AVAILABLE_IN_3_32
IdeCloneSurface *ide_clone_surface_new     (void);
IDE_AVAILABLE_IN_3_32
const gchar     *ide_clone_surface_get_uri (IdeCloneSurface *self);
IDE_AVAILABLE_IN_3_32
void             ide_clone_surface_set_uri (IdeCloneSurface *self,
                                            const gchar     *uri);
IDE_AVAILABLE_IN_3_32
void             ide_clone_surface_clone   (IdeCloneSurface *self);

G_END_DECLS
