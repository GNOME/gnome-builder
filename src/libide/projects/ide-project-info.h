/* ide-project-info.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include <gio/gio.h>

#include "doap/ide-doap.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_INFO (ide_project_info_get_type())

G_DECLARE_FINAL_TYPE (IdeProjectInfo, ide_project_info, IDE, PROJECT_INFO, GObject)

gint         ide_project_info_compare                (IdeProjectInfo  *info1,
                                                      IdeProjectInfo  *info2);
GFile        *ide_project_info_get_file              (IdeProjectInfo  *self);
IdeDoap      *ide_project_info_get_doap              (IdeProjectInfo  *self);
const gchar  *ide_project_info_get_build_system_name (IdeProjectInfo  *self);
const gchar  *ide_project_info_get_description       (IdeProjectInfo  *self);
GFile        *ide_project_info_get_directory         (IdeProjectInfo  *self);
gboolean      ide_project_info_get_is_recent         (IdeProjectInfo  *self);
gint          ide_project_info_get_priority          (IdeProjectInfo  *self);
GDateTime    *ide_project_info_get_last_modified_at  (IdeProjectInfo  *self);
const gchar * const *
              ide_project_info_get_languages         (IdeProjectInfo  *self);
const gchar  *ide_project_info_get_name              (IdeProjectInfo  *self);
void          ide_project_info_set_file              (IdeProjectInfo  *self,
                                                      GFile           *file);
void          ide_project_info_set_build_system_name (IdeProjectInfo  *self,
                                                      const gchar     *build_system_name);
void          ide_project_info_set_description       (IdeProjectInfo  *self,
                                                      const gchar     *description);
void          ide_project_info_set_directory         (IdeProjectInfo  *self,
                                                      GFile           *directory);
void          ide_project_info_set_is_recent         (IdeProjectInfo  *self,
                                                      gboolean         is_recent);
void          ide_project_info_set_languages         (IdeProjectInfo  *self,
                                                      gchar          **languages);
void          ide_project_info_set_name              (IdeProjectInfo  *self,
                                                      const gchar     *name);
void          ide_project_info_set_priority          (IdeProjectInfo  *self,
                                                      gint             priority);

G_END_DECLS
