/* ide-build-configuration-row.h
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>
#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_CONFIGURATION_ROW (ide_build_configuration_row_get_type())

G_DECLARE_FINAL_TYPE (IdeBuildConfigurationRow, ide_build_configuration_row, IDE, BUILD_CONFIGURATION_ROW, GtkListBoxRow)

GtkWidget        *ide_build_configuration_row_new               (IdeConfiguration         *configuration);
IdeConfiguration *ide_build_configuration_row_get_configuration (IdeBuildConfigurationRow *self);

G_END_DECLS
