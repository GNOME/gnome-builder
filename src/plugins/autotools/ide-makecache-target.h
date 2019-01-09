/* ide-makecache-target.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_MAKECACHE_TARGET (ide_makecache_target_get_type())

typedef struct _IdeMakecacheTarget IdeMakecacheTarget;

GType               ide_makecache_target_get_type   (void);
IdeMakecacheTarget *ide_makecache_target_new        (const gchar        *subdir,
                                                     const gchar        *target);
IdeMakecacheTarget *ide_makecache_target_ref        (IdeMakecacheTarget *self);
void                ide_makecache_target_unref      (IdeMakecacheTarget *self);
const gchar        *ide_makecache_target_get_target (IdeMakecacheTarget *self);
const gchar        *ide_makecache_target_get_subdir (IdeMakecacheTarget *self);
void                ide_makecache_target_set_target (IdeMakecacheTarget *self,
                                                     const gchar        *target);
guint               ide_makecache_target_hash       (gconstpointer       data);
gboolean            ide_makecache_target_equal      (gconstpointer       data1,
                                                     gconstpointer       data2);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeMakecacheTarget, ide_makecache_target_unref)

G_END_DECLS
