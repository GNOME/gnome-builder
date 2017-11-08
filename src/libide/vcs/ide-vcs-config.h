/* ide-vcs-config.h
 *
 * Copyright © 2016 Akshaya Kakkilaya <akshaya.kakkilaya@gmail.com>
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

#include <glib-object.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_VCS_CONFIG (ide_vcs_config_get_type())

G_DECLARE_INTERFACE (IdeVcsConfig, ide_vcs_config, IDE, VCS_CONFIG, GObject)

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
