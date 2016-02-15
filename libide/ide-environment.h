/* ide-environment.h
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

#ifndef IDE_ENVIRONMENT_H
#define IDE_ENVIRONMENT_H

#include <gio/gio.h>

#include "ide-environment-variable.h"

G_BEGIN_DECLS

#define IDE_TYPE_ENVIRONMENT (ide_environment_get_type())

G_DECLARE_FINAL_TYPE (IdeEnvironment, ide_environment, IDE, ENVIRONMENT, GObject)

IdeEnvironment *ide_environment_new         (void);
void            ide_environment_setenv      (IdeEnvironment         *self,
                                             const gchar            *key,
                                             const gchar            *value);
const gchar    *ide_environment_getenv      (IdeEnvironment         *self,
                                             const gchar            *key);
gchar         **ide_environment_get_environ (IdeEnvironment         *self);
void            ide_environment_append      (IdeEnvironment         *self,
                                             IdeEnvironmentVariable *variable);
void            ide_environment_remove      (IdeEnvironment         *self,
                                             IdeEnvironmentVariable *variable);
IdeEnvironment *ide_environment_copy        (IdeEnvironment         *self);

G_END_DECLS

#endif /* IDE_ENVIRONMENT_H */
