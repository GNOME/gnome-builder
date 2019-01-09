/* ide-configuration.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>
#include <libide-threading.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONFIGURATION (ide_configuration_get_type())

IDE_AVAILABLE_IN_3_32
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

IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_get_append_path           (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_append_path           (IdeConfiguration      *self,
                                                                   const gchar           *append_path);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_get_id                    (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_get_runtime_id            (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_runtime_id            (IdeConfiguration      *self,
                                                                   const gchar           *runtime_id);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_get_toolchain_id          (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_toolchain_id          (IdeConfiguration      *self,
                                                                   const gchar           *toolchain_id);
IDE_AVAILABLE_IN_3_32
gboolean              ide_configuration_get_dirty                 (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_dirty                 (IdeConfiguration      *self,
                                                                   gboolean               dirty);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_get_display_name          (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_display_name          (IdeConfiguration      *self,
                                                                   const gchar           *display_name);
IDE_AVAILABLE_IN_3_32
IdeBuildLocality      ide_configuration_get_locality              (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_locality              (IdeConfiguration      *self,
                                                                   IdeBuildLocality       locality);
IDE_AVAILABLE_IN_3_32
gboolean              ide_configuration_get_ready                 (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
IdeRuntime           *ide_configuration_get_runtime               (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_runtime               (IdeConfiguration      *self,
                                                                   IdeRuntime            *runtime);
IDE_AVAILABLE_IN_3_32
IdeToolchain         *ide_configuration_get_toolchain             (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_toolchain             (IdeConfiguration      *self,
                                                                   IdeToolchain          *toolchain);
IDE_AVAILABLE_IN_3_32
gchar               **ide_configuration_get_environ               (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_getenv                    (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_setenv                    (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   const gchar           *value);
IDE_AVAILABLE_IN_3_32
gboolean              ide_configuration_get_debug                 (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_debug                 (IdeConfiguration      *self,
                                                                   gboolean               debug);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_get_prefix                (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_prefix                (IdeConfiguration      *self,
                                                                   const gchar           *prefix);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_get_config_opts           (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_config_opts           (IdeConfiguration      *self,
                                                                   const gchar           *config_opts);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_get_run_opts              (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_run_opts              (IdeConfiguration      *self,
                                                                   const gchar           *run_opts);
IDE_AVAILABLE_IN_3_32
const gchar * const  *ide_configuration_get_build_commands        (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_build_commands        (IdeConfiguration      *self,
                                                                   const gchar *const    *build_commands);
IDE_AVAILABLE_IN_3_32
GFile                *ide_configuration_get_build_commands_dir    (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_build_commands_dir    (IdeConfiguration      *self,
                                                                   GFile                 *build_commands_dir);
IDE_AVAILABLE_IN_3_32
const gchar * const  *ide_configuration_get_post_install_commands (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_post_install_commands (IdeConfiguration      *self,
                                                                   const gchar *const    *post_install_commands);
IDE_AVAILABLE_IN_3_32
gint                  ide_configuration_get_parallelism           (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_parallelism           (IdeConfiguration      *self,
                                                                   gint                   parallelism);
IDE_AVAILABLE_IN_3_32
IdeEnvironment       *ide_configuration_get_environment           (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_environment           (IdeConfiguration      *self,
                                                                   IdeEnvironment        *environment);
IDE_AVAILABLE_IN_3_32
guint                 ide_configuration_get_sequence              (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_get_app_id                (IdeConfiguration      *self);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_app_id                (IdeConfiguration      *self,
                                                                   const gchar           *app_id);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_apply_path                (IdeConfiguration      *self,
                                                                   IdeSubprocessLauncher *launcher);
IDE_AVAILABLE_IN_3_32
gboolean              ide_configuration_supports_runtime          (IdeConfiguration      *self,
                                                                   IdeRuntime            *runtime);
IDE_AVAILABLE_IN_3_32
const gchar          *ide_configuration_get_internal_string       (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_internal_string       (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   const gchar           *value);
IDE_AVAILABLE_IN_3_32
const gchar * const  *ide_configuration_get_internal_strv         (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_internal_strv         (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   const gchar *const    *value);
IDE_AVAILABLE_IN_3_32
gboolean              ide_configuration_get_internal_boolean      (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_internal_boolean      (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gboolean               value);
IDE_AVAILABLE_IN_3_32
gint                  ide_configuration_get_internal_int          (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_internal_int          (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gint                   value);
IDE_AVAILABLE_IN_3_32
gint64                ide_configuration_get_internal_int64        (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_internal_int64        (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gint64                 value);
IDE_AVAILABLE_IN_3_32
gpointer              ide_configuration_get_internal_object       (IdeConfiguration      *self,
                                                                   const gchar           *key);
IDE_AVAILABLE_IN_3_32
void                  ide_configuration_set_internal_object       (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gpointer               instance);

G_END_DECLS
