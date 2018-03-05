/* ide-configuration.h
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

#include <gio/gio.h>

#include "ide-version-macros.h"

#include "ide-object.h"
#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONFIGURATION (ide_configuration_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeConfiguration, ide_configuration, IDE, CONFIGURATION, IdeObject)

typedef enum
{
  IDE_BUILD_LOCALITY_IN_TREE     = 1 << 0,
  IDE_BUILD_LOCALITY_OUT_OF_TREE = 1 << 1,
  IDE_BUILD_LOCALITY_DEFAULT     = IDE_BUILD_LOCALITY_IN_TREE | IDE_BUILD_LOCALITY_OUT_OF_TREE,
} IdeBuildLocality;

struct _IdeConfigurationClass
{
  IdeObjectClass parent;

  IdeRuntime *(*get_runtime)      (IdeConfiguration *self);
  void        (*set_runtime)      (IdeConfiguration *self,
                                   IdeRuntime       *runtime);
  gboolean    (*supports_runtime) (IdeConfiguration *self,
                                   IdeRuntime       *runtime);

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_3_28
const gchar          *ide_configuration_get_append_path           (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_28
void                  ide_configuration_set_append_path           (IdeConfiguration      *self,
                                                                   const gchar           *append_path);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_configuration_get_id                    (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_configuration_get_runtime_id            (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_runtime_id            (IdeConfiguration      *self,
                                                                   const gchar           *runtime_id);
IDE_AVAILABLE_IN_ALL
gboolean              ide_configuration_get_dirty                 (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_dirty                 (IdeConfiguration      *self,
                                                                   gboolean               dirty);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_configuration_get_display_name          (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_display_name          (IdeConfiguration      *self,
                                                                   const gchar           *display_name);
IDE_AVAILABLE_IN_3_28
IdeBuildLocality      ide_configuration_get_locality              (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_28
void                  ide_configuration_set_locality              (IdeConfiguration      *self,
                                                                   IdeBuildLocality       locality);
IDE_AVAILABLE_IN_ALL
gboolean              ide_configuration_get_ready                 (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
IdeRuntime           *ide_configuration_get_runtime               (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_runtime               (IdeConfiguration      *self,
                                                                   IdeRuntime            *runtime);
IDE_AVAILABLE_IN_ALL
gchar               **ide_configuration_get_environ               (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_configuration_getenv                    (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_setenv                    (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   const gchar           *value);
IDE_AVAILABLE_IN_ALL
gboolean              ide_configuration_get_debug                 (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_debug                 (IdeConfiguration      *self,
                                                                   gboolean               debug);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_configuration_get_prefix                (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_prefix                (IdeConfiguration      *self,
                                                                   const gchar           *prefix);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_configuration_get_config_opts           (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_config_opts           (IdeConfiguration      *self,
                                                                   const gchar           *config_opts);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_configuration_get_run_opts              (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_run_opts              (IdeConfiguration      *self,
                                                                   const gchar           *run_opts);
IDE_AVAILABLE_IN_ALL
const gchar * const  *ide_configuration_get_build_commands        (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_build_commands        (IdeConfiguration      *self,
                                                                   const gchar *const    *build_commands);
IDE_AVAILABLE_IN_ALL
const gchar * const  *ide_configuration_get_post_install_commands (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_post_install_commands (IdeConfiguration      *self,
                                                                   const gchar *const    *post_install_commands);
IDE_AVAILABLE_IN_ALL
gint                  ide_configuration_get_parallelism           (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_parallelism           (IdeConfiguration      *self,
                                                                   gint                   parallelism);
IDE_AVAILABLE_IN_ALL
IdeEnvironment       *ide_configuration_get_environment           (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_environment           (IdeConfiguration      *self,
                                                                   IdeEnvironment        *environment);
IDE_AVAILABLE_IN_ALL
guint                 ide_configuration_get_sequence              (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_configuration_get_app_id                (IdeConfiguration      *self);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_app_id                (IdeConfiguration      *self,
                                                                   const gchar           *app_id);
IDE_AVAILABLE_IN_3_28
void                  ide_configuration_apply_path                (IdeConfiguration      *self,
                                                                   IdeSubprocessLauncher *launcher);
IDE_AVAILABLE_IN_ALL
gboolean              ide_configuration_supports_runtime          (IdeConfiguration      *self,
                                                                   IdeRuntime            *runtime);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_configuration_get_internal_string       (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_internal_string       (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   const gchar           *value);
IDE_AVAILABLE_IN_ALL
const gchar * const  *ide_configuration_get_internal_strv         (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_internal_strv         (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   const gchar *const    *value);
IDE_AVAILABLE_IN_ALL
gboolean              ide_configuration_get_internal_boolean      (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_internal_boolean      (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gboolean               value);
IDE_AVAILABLE_IN_ALL
gint                  ide_configuration_get_internal_int          (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_internal_int          (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gint                   value);
IDE_AVAILABLE_IN_ALL
gint64                ide_configuration_get_internal_int64        (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_internal_int64        (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gint64                 value);
IDE_AVAILABLE_IN_ALL
gpointer              ide_configuration_get_internal_object       (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_configuration_set_internal_object       (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gpointer               instance);

G_END_DECLS
