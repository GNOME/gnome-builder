/* ide-posix.h
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

#include <glib.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

IDE_AVAILABLE_IN_ALL
gchar       *ide_get_system_arch      (void);
IDE_AVAILABLE_IN_ALL
const gchar *ide_get_system_type      (void);
IDE_AVAILABLE_IN_3_28
gchar       *ide_create_host_triplet  (const gchar *arch,
                                       const gchar *kernel,
                                       const gchar *system);
IDE_AVAILABLE_IN_ALL
gsize        ide_get_system_page_size (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_ALL
gchar       *ide_path_collapse        (const gchar *path);
IDE_AVAILABLE_IN_ALL
gchar       *ide_path_expand          (const gchar *path);

G_END_DECLS
