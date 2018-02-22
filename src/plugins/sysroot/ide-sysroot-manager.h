/* ide-sysroot-manager.h
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
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

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_SYSROOT_MANAGER (ide_sysroot_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeSysrootManager, ide_sysroot_manager, IDE, SYSROOT_MANAGER, GObject)

typedef enum {
  IDE_SYSROOT_MANAGER_TARGET_CHANGED,
  IDE_SYSROOT_MANAGER_TARGET_CREATED,
  IDE_SYSROOT_MANAGER_TARGET_REMOVED
} IdeSysrootManagerTargetModificationType;

IdeSysrootManager        *ide_sysroot_manager_get_default                 (void);
gchar                    *ide_sysroot_manager_create_target               (IdeSysrootManager *self);
void                      ide_sysroot_manager_remove_target               (IdeSysrootManager *self,
                                                                           const gchar       *target);
void                      ide_sysroot_manager_set_target_name             (IdeSysrootManager *self,
                                                                           const gchar       *target,
                                                                           const gchar       *path);
gchar                    *ide_sysroot_manager_get_target_name             (IdeSysrootManager *self,
                                                                           const gchar       *target);
void                      ide_sysroot_manager_set_target_path             (IdeSysrootManager *self,
                                                                           const gchar       *target,
                                                                           const gchar       *path);
gchar                    *ide_sysroot_manager_get_target_path             (IdeSysrootManager *self,
                                                                           const gchar       *target);
void                      ide_sysroot_manager_set_target_pkg_config_path  (IdeSysrootManager *self,
                                                                           const gchar       *target,
                                                                           const gchar       *path);
gchar                    *ide_sysroot_manager_get_target_pkg_config_path  (IdeSysrootManager *self,
                                                                           const gchar       *target);
gchar                    **ide_sysroot_manager_list                       (IdeSysrootManager *self);

G_END_DECLS
