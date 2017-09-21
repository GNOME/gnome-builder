/* ide-environment-variable.h
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_ENVIRONMENT_VARIABLE_H
#define IDE_ENVIRONMENT_VARIABLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_ENVIRONMENT_VARIABLE (ide_environment_variable_get_type())

G_DECLARE_FINAL_TYPE (IdeEnvironmentVariable, ide_environment_variable, IDE, ENVIRONMENT_VARIABLE, GObject)

IdeEnvironmentVariable *ide_environment_variable_new       (const gchar            *key,
                                                            const gchar            *value);
const gchar            *ide_environment_variable_get_key   (IdeEnvironmentVariable *self);
void                    ide_environment_variable_set_key   (IdeEnvironmentVariable *self,
                                                            const gchar            *key);
const gchar            *ide_environment_variable_get_value (IdeEnvironmentVariable *self);
void                    ide_environment_variable_set_value (IdeEnvironmentVariable *self,
                                                            const gchar            *value);

G_END_DECLS

#endif /* IDE_ENVIRONMENT_VARIABLE_H */
