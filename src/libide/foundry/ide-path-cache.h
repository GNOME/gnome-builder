/* ide-path-cache.h
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_PATH_CACHE (ide_path_cache_get_type())

IDE_AVAILABLE_IN_44
G_DECLARE_FINAL_TYPE (IdePathCache, ide_path_cache, IDE, PATH_CACHE, GObject)

IDE_AVAILABLE_IN_44
IdePathCache *ide_path_cache_new      (void);
IDE_AVAILABLE_IN_44
gboolean      ide_path_cache_lookup   (IdePathCache  *self,
                                       const char    *program_name,
                                       char         **program_path);
IDE_AVAILABLE_IN_44
gboolean      ide_path_cache_contains (IdePathCache  *self,
                                       const char    *program_name,
                                       gboolean      *had_program_path);
IDE_AVAILABLE_IN_44
void          ide_path_cache_insert   (IdePathCache  *self,
                                       const char    *program_name,
                                       const char    *program_path);

G_END_DECLS
