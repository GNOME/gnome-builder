/* ide-configuration.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_CONFIGURATION_H
#define IDE_CONFIGURATION_H

#include <gio/gio.h>

#include "ide-object.h"
#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONFIGURATION (ide_configuration_get_type())

G_DECLARE_FINAL_TYPE (IdeConfiguration, ide_configuration, IDE, CONFIGURATION, IdeObject)

IdeConfiguration    *ide_configuration_new              (IdeContext        *context,
                                                         const gchar       *id,
                                                         const gchar       *device_id,
                                                         const gchar       *runtime_id);
const gchar         *ide_configuration_get_id           (IdeConfiguration  *self);
const gchar         *ide_configuration_get_runtime_id   (IdeConfiguration  *self);
void                 ide_configuration_set_runtime_id   (IdeConfiguration  *self,
                                                         const gchar       *runtime_id);
const gchar         *ide_configuration_get_device_id    (IdeConfiguration  *self);
void                 ide_configuration_set_device_id    (IdeConfiguration  *self,
                                                         const gchar       *device_id);
IdeDevice           *ide_configuration_get_device       (IdeConfiguration  *self);
void                 ide_configuration_set_device       (IdeConfiguration  *self,
                                                         IdeDevice         *device);
gboolean             ide_configuration_get_dirty        (IdeConfiguration  *self);
void                 ide_configuration_set_dirty        (IdeConfiguration  *self,
                                                         gboolean           dirty);
const gchar         *ide_configuration_get_display_name (IdeConfiguration  *self);
void                 ide_configuration_set_display_name (IdeConfiguration  *self,
                                                         const gchar       *display_name);
IdeRuntime          *ide_configuration_get_runtime      (IdeConfiguration  *self);
void                 ide_configuration_set_runtime      (IdeConfiguration  *self,
                                                         IdeRuntime        *runtime);
gchar              **ide_configuration_get_environ      (IdeConfiguration  *self);
const gchar         *ide_configuration_getenv           (IdeConfiguration  *self,
                                                         const gchar       *key);
void                 ide_configuration_setenv           (IdeConfiguration  *self,
                                                         const gchar       *key,
                                                         const gchar       *value);
gboolean             ide_configuration_get_debug        (IdeConfiguration  *self);
void                 ide_configuration_set_debug        (IdeConfiguration  *self,
                                                         gboolean           debug);
const gchar         *ide_configuration_get_prefix       (IdeConfiguration  *self);
void                 ide_configuration_set_prefix       (IdeConfiguration  *self,
                                                         const gchar       *prefix);
const gchar         *ide_configuration_get_config_opts  (IdeConfiguration  *self);
void                 ide_configuration_set_config_opts  (IdeConfiguration  *self,
                                                         const gchar       *config_opts);
gint                 ide_configuration_get_parallelism  (IdeConfiguration  *self);
void                 ide_configuration_set_parallelism  (IdeConfiguration  *self,
                                                         gint               parallelism);
IdeEnvironment      *ide_configuration_get_environment  (IdeConfiguration  *self);
IdeConfiguration    *ide_configuration_duplicate        (IdeConfiguration  *self);

G_END_DECLS

#endif /* IDE_CONFIGURATION_H */
