/* ide-environment.h
 *
 * Copyright 2016 Christian Hergert <christian@hergert.me>
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

#include <gio/gio.h>

#include "ide-version-macros.h"

#include "buildsystem/ide-environment-variable.h"

G_BEGIN_DECLS

#define IDE_TYPE_ENVIRONMENT (ide_environment_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeEnvironment, ide_environment, IDE, ENVIRONMENT, GObject)

IDE_AVAILABLE_IN_ALL
IdeEnvironment *ide_environment_new         (void);
IDE_AVAILABLE_IN_ALL
void            ide_environment_setenv      (IdeEnvironment         *self,
                                             const gchar            *key,
                                             const gchar            *value);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_environment_getenv      (IdeEnvironment         *self,
                                             const gchar            *key);
IDE_AVAILABLE_IN_ALL
gchar         **ide_environment_get_environ (IdeEnvironment         *self);
IDE_AVAILABLE_IN_ALL
void            ide_environment_append      (IdeEnvironment         *self,
                                             IdeEnvironmentVariable *variable);
IDE_AVAILABLE_IN_ALL
void            ide_environment_remove      (IdeEnvironment         *self,
                                             IdeEnvironmentVariable *variable);
IDE_AVAILABLE_IN_ALL
IdeEnvironment *ide_environment_copy        (IdeEnvironment         *self);
IDE_AVAILABLE_IN_ALL
void            ide_environment_copy_into   (IdeEnvironment         *self,
                                             IdeEnvironment         *dest,
                                             gboolean                replace);

G_END_DECLS
