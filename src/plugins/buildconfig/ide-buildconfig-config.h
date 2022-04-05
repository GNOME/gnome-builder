/* ide-buildconfig-config.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_BUILDCONFIG_CONFIG (ide_buildconfig_config_get_type())

G_DECLARE_FINAL_TYPE (IdeBuildconfigConfig, ide_buildconfig_config, IDE, BUILDCONFIG_CONFIG, IdeConfig)

const gchar * const *ide_buildconfig_config_get_prebuild    (IdeBuildconfigConfig *self);
void                 ide_buildconfig_config_set_prebuild    (IdeBuildconfigConfig *self,
                                                             const gchar * const  *prebuild);
const gchar * const *ide_buildconfig_config_get_postbuild   (IdeBuildconfigConfig *self);
void                 ide_buildconfig_config_set_postbuild   (IdeBuildconfigConfig *self,
                                                             const gchar * const  *postbuild);
const gchar * const *ide_buildconfig_config_get_run_command (IdeBuildconfigConfig *self);
void                 ide_buildconfig_config_set_run_command (IdeBuildconfigConfig *self,
                                                             const gchar * const  *postbuild);

G_END_DECLS
