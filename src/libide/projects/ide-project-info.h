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

#include "ide-version-macros.h"

#include "doap/ide-doap.h"
#include "vcs/ide-vcs-uri.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_INFO (ide_project_info_get_type())

G_DECLARE_FINAL_TYPE (IdeProjectInfo, ide_project_info, IDE, PROJECT_INFO, GObject)

IDE_AVAILABLE_IN_ALL
gint         ide_project_info_compare                (IdeProjectInfo  *info1,
                                                      IdeProjectInfo  *info2);
IDE_AVAILABLE_IN_ALL
GFile        *ide_project_info_get_file              (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_ALL
IdeDoap      *ide_project_info_get_doap              (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_ALL
const gchar  *ide_project_info_get_build_system_name (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_ALL
const gchar  *ide_project_info_get_description       (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_ALL
GFile        *ide_project_info_get_directory         (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_ALL
gboolean      ide_project_info_get_is_recent         (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_ALL
gint          ide_project_info_get_priority          (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_ALL
GDateTime    *ide_project_info_get_last_modified_at  (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_ALL
const gchar * const *
              ide_project_info_get_languages         (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_ALL
const gchar  *ide_project_info_get_name              (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_3_28
IdeVcsUri    *ide_project_info_get_vcs_uri           (IdeProjectInfo  *self);
IDE_AVAILABLE_IN_ALL
void          ide_project_info_set_file              (IdeProjectInfo  *self,
                                                      GFile           *file);
IDE_AVAILABLE_IN_ALL
void          ide_project_info_set_build_system_name (IdeProjectInfo  *self,
                                                      const gchar     *build_system_name);
IDE_AVAILABLE_IN_ALL
void          ide_project_info_set_description       (IdeProjectInfo  *self,
                                                      const gchar     *description);
IDE_AVAILABLE_IN_ALL
void          ide_project_info_set_directory         (IdeProjectInfo  *self,
                                                      GFile           *directory);
IDE_AVAILABLE_IN_ALL
void          ide_project_info_set_is_recent         (IdeProjectInfo  *self,
                                                      gboolean         is_recent);
IDE_AVAILABLE_IN_ALL
void          ide_project_info_set_languages         (IdeProjectInfo  *self,
                                                      gchar          **languages);
IDE_AVAILABLE_IN_ALL
void          ide_project_info_set_name              (IdeProjectInfo  *self,
                                                      const gchar     *name);
IDE_AVAILABLE_IN_ALL
void          ide_project_info_set_priority          (IdeProjectInfo  *self,
                                                      gint             priority);
IDE_AVAILABLE_IN_3_28
void          ide_project_info_set_vcs_uri           (IdeProjectInfo  *self,
                                                      IdeVcsUri       *uri);

G_END_DECLS
