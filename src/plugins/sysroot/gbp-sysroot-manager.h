/* gbp-sysroot-manager.h
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#include <libide-foundry.h>

G_BEGIN_DECLS

#define GBP_TYPE_SYSROOT_MANAGER (gbp_sysroot_manager_get_type())

G_DECLARE_FINAL_TYPE (GbpSysrootManager, gbp_sysroot_manager, GBP, SYSROOT_MANAGER, GObject)

typedef enum {
  GBP_SYSROOT_MANAGER_TARGET_CHANGED,
  GBP_SYSROOT_MANAGER_TARGET_CREATED,
  GBP_SYSROOT_MANAGER_TARGET_REMOVED
} GbpSysrootManagerTargetModificationType;

GbpSysrootManager        *gbp_sysroot_manager_get_default                 (void);
gchar                    *gbp_sysroot_manager_create_target               (GbpSysrootManager *self);
void                      gbp_sysroot_manager_remove_target               (GbpSysrootManager *self,
                                                                           const gchar       *target);
void                      gbp_sysroot_manager_set_target_name             (GbpSysrootManager *self,
                                                                           const gchar       *target,
                                                                           const gchar       *path);
gchar                    *gbp_sysroot_manager_get_target_name             (GbpSysrootManager *self,
                                                                           const gchar       *target);
void                      gbp_sysroot_manager_set_target_arch             (GbpSysrootManager *self,
                                                                           const gchar       *target,
                                                                           const gchar       *arch);
gchar                    *gbp_sysroot_manager_get_target_arch             (GbpSysrootManager *self,
                                                                           const gchar       *target);
void                      gbp_sysroot_manager_set_target_path             (GbpSysrootManager *self,
                                                                           const gchar       *target,
                                                                           const gchar       *path);
gchar                    *gbp_sysroot_manager_get_target_path             (GbpSysrootManager *self,
                                                                           const gchar       *target);
void                      gbp_sysroot_manager_set_target_pkg_config_path  (GbpSysrootManager *self,
                                                                           const gchar       *target,
                                                                           const gchar       *path);
gchar                    *gbp_sysroot_manager_get_target_pkg_config_path  (GbpSysrootManager *self,
                                                                           const gchar       *target);
gchar                   **gbp_sysroot_manager_list                        (GbpSysrootManager *self);

G_END_DECLS
