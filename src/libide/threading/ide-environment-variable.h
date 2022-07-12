/* ide-environment-variable.h
 *
 * Copyright 2016-2019 Christian Hergert <christian@hergert.me>
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

#if !defined (IDE_THREADING_INSIDE) && !defined (IDE_THREADING_COMPILATION)
# error "Only <libide-threading.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_ENVIRONMENT_VARIABLE (ide_environment_variable_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeEnvironmentVariable, ide_environment_variable, IDE, ENVIRONMENT_VARIABLE, GObject)

IDE_AVAILABLE_IN_ALL
IdeEnvironmentVariable *ide_environment_variable_new       (const gchar            *key,
                                                            const gchar            *value);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_environment_variable_get_key   (IdeEnvironmentVariable *self);
IDE_AVAILABLE_IN_ALL
void                    ide_environment_variable_set_key   (IdeEnvironmentVariable *self,
                                                            const gchar            *key);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_environment_variable_get_value (IdeEnvironmentVariable *self);
IDE_AVAILABLE_IN_ALL
void                    ide_environment_variable_set_value (IdeEnvironmentVariable *self,
                                                            const gchar            *value);

G_END_DECLS
