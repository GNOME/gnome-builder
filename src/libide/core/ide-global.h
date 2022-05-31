/* ide-global.h
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_CORE_INSIDE) && !defined (IDE_CORE_COMPILATION)
# error "Only <libide-core.h> can be included directly."
#endif

#include <glib.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

typedef enum
{
  IDE_PROCESS_KIND_HOST    = 0,
  IDE_PROCESS_KIND_FLATPAK = 1,
} IdeProcessKind;

#ifdef __linux__
# define ide_is_flatpak() (ide_get_process_kind() == IDE_PROCESS_KIND_FLATPAK)
#else
# define ide_is_flatpak() 0
#endif

IDE_AVAILABLE_IN_ALL
const gchar    *ide_gettext              (const gchar *message);
IDE_AVAILABLE_IN_ALL
GThread        *ide_get_main_thread      (void);
IDE_AVAILABLE_IN_ALL
IdeProcessKind  ide_get_process_kind     (void);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_get_application_id   (void);
IDE_AVAILABLE_IN_ALL
void            ide_set_application_id   (const gchar *app_id);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_get_program_name     (void);
IDE_AVAILABLE_IN_ALL
gchar          *ide_get_system_arch      (void);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_get_system_type      (void);
IDE_AVAILABLE_IN_ALL
gchar          *ide_create_host_triplet  (const gchar *arch,
                                          const gchar *kernel,
                                          const gchar *system);
IDE_AVAILABLE_IN_ALL
gsize           ide_get_system_page_size (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_ALL
gchar          *ide_get_relocatable_path (const gchar *path);
IDE_AVAILABLE_IN_ALL
const char     *ide_get_projects_dir     (void);

G_END_DECLS
