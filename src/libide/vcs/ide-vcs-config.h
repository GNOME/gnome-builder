/* ide-vcs-config.h
 *
 * Copyright 2016 Akshaya Kakkilaya <akshaya.kakkilaya@gmail.com>
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

#if !defined (IDE_VCS_INSIDE) && !defined (IDE_VCS_COMPILATION)
# error "Only <libide-vcs.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_VCS_CONFIG (ide_vcs_config_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeVcsConfig, ide_vcs_config, IDE, VCS_CONFIG, IdeObject)

typedef enum
{
  IDE_VCS_CONFIG_FULL_NAME,
  IDE_VCS_CONFIG_EMAIL,
} IdeVcsConfigType;

struct _IdeVcsConfigInterface
{
  GTypeInterface parent;

  void (*get_config) (IdeVcsConfig    *self,
                      IdeVcsConfigType type,
                      GValue          *value);
  void (*set_config) (IdeVcsConfig    *self,
                      IdeVcsConfigType type,
                      const GValue    *value);
};

IDE_AVAILABLE_IN_ALL
void ide_vcs_config_get_config (IdeVcsConfig     *self,
                                IdeVcsConfigType  type,
                                GValue           *value);
IDE_AVAILABLE_IN_ALL
void ide_vcs_config_set_config (IdeVcsConfig     *self,
                                IdeVcsConfigType  type,
                                const GValue     *value);

G_END_DECLS
