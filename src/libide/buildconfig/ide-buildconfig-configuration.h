/* ide-buildconfig-configuration.h
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#include "ide-version-macros.h"

#include "config/ide-configuration.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILDCONFIG_CONFIGURATION (ide_buildconfig_configuration_get_type())

G_DECLARE_FINAL_TYPE (IdeBuildconfigConfiguration, ide_buildconfig_configuration, IDE, BUILDCONFIG_CONFIGURATION, IdeConfiguration)

IDE_AVAILABLE_IN_ALL
const gchar * const *ide_buildconfig_configuration_get_prebuild  (IdeBuildconfigConfiguration *self);
IDE_AVAILABLE_IN_ALL
void                 ide_buildconfig_configuration_set_prebuild  (IdeBuildconfigConfiguration *self,
                                                                  const gchar * const         *prebuild);
IDE_AVAILABLE_IN_ALL
const gchar * const *ide_buildconfig_configuration_get_postbuild (IdeBuildconfigConfiguration *self);
IDE_AVAILABLE_IN_ALL
void                 ide_buildconfig_configuration_set_postbuild (IdeBuildconfigConfiguration *self,
                                                                  const gchar * const         *postbuild);

G_END_DECLS
