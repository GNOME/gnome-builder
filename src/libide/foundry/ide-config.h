/* ide-config.h
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
#include "ide-pipeline-phase.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONFIG (ide_config_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeConfig, ide_config, IDE, CONFIG, IdeObject)

typedef enum
{
  IDE_BUILD_LOCALITY_IN_TREE     = 1 << 0,
  IDE_BUILD_LOCALITY_OUT_OF_TREE = 1 << 1,
  IDE_BUILD_LOCALITY_DEFAULT     = IDE_BUILD_LOCALITY_IN_TREE | IDE_BUILD_LOCALITY_OUT_OF_TREE,
} IdeBuildLocality;

struct _IdeConfigClass
{
  IdeObjectClass parent;

  IdeRuntime *(*get_runtime)      (IdeConfig  *self);
  void        (*set_runtime)      (IdeConfig  *self,
                                   IdeRuntime *runtime);
  gboolean    (*supports_runtime) (IdeConfig  *self,
                                   IdeRuntime *runtime);
  GPtrArray  *(*get_extensions)   (IdeConfig  *self);
  char       *(*get_description)  (IdeConfig  *self);
  GFile      *(*translate_file)   (IdeConfig  *self,
                                   GFile      *file);

  /*< private >*/
  gpointer _reserved[14];
};

IDE_AVAILABLE_IN_ALL
char                 *ide_config_get_description           (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_prepend_path          (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_prepend_path          (IdeConfig             *self,
                                                            const gchar           *prepend_path);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_append_path           (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_append_path           (IdeConfig             *self,
                                                            const gchar           *append_path);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_id                    (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_runtime_id            (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_runtime_id            (IdeConfig             *self,
                                                            const gchar           *runtime_id);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_toolchain_id          (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_toolchain_id          (IdeConfig             *self,
                                                            const gchar           *toolchain_id);
IDE_AVAILABLE_IN_ALL
gboolean              ide_config_get_dirty                 (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_dirty                 (IdeConfig             *self,
                                                            gboolean               dirty);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_display_name          (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_display_name          (IdeConfig             *self,
                                                            const gchar           *display_name);
IDE_AVAILABLE_IN_ALL
IdeBuildLocality      ide_config_get_locality              (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_locality              (IdeConfig             *self,
                                                            IdeBuildLocality       locality);
IDE_AVAILABLE_IN_ALL
gboolean              ide_config_get_ready                 (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
IdeRuntime           *ide_config_get_runtime               (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_runtime               (IdeConfig             *self,
                                                            IdeRuntime            *runtime);
IDE_AVAILABLE_IN_ALL
IdeToolchain         *ide_config_get_toolchain             (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_toolchain             (IdeConfig             *self,
                                                            IdeToolchain          *toolchain);
IDE_AVAILABLE_IN_ALL
gchar               **ide_config_get_environ               (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_getenv                    (IdeConfig             *self,
                                                            const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_config_setenv                    (IdeConfig             *self,
                                                            const gchar           *key,
                                                            const gchar           *value);
IDE_AVAILABLE_IN_ALL
gboolean              ide_config_get_debug                 (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_debug                 (IdeConfig             *self,
                                                            gboolean               debug);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_prefix                (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_prefix                (IdeConfig             *self,
                                                            const gchar           *prefix);
IDE_AVAILABLE_IN_ALL
gboolean              ide_config_get_prefix_set            (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_prefix_set            (IdeConfig             *self,
                                                            gboolean               prefix_set);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_config_opts           (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_config_opts           (IdeConfig             *self,
                                                            const gchar           *config_opts);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_run_opts              (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_run_opts              (IdeConfig             *self,
                                                            const gchar           *run_opts);
IDE_AVAILABLE_IN_ALL
const gchar * const  *ide_config_get_build_commands        (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_build_commands        (IdeConfig             *self,
                                                            const gchar *const    *build_commands);
IDE_AVAILABLE_IN_ALL
GFile                *ide_config_get_build_commands_dir    (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_build_commands_dir    (IdeConfig             *self,
                                                            GFile                 *build_commands_dir);
IDE_AVAILABLE_IN_ALL
const gchar * const  *ide_config_get_post_install_commands (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_post_install_commands (IdeConfig             *self,
                                                            const gchar *const    *post_install_commands);
IDE_AVAILABLE_IN_ALL
gint                  ide_config_get_parallelism           (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_parallelism           (IdeConfig             *self,
                                                            gint                   parallelism);
IDE_AVAILABLE_IN_ALL
IdeEnvironment       *ide_config_get_environment           (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_environment           (IdeConfig             *self,
                                                            IdeEnvironment        *environment);
IDE_AVAILABLE_IN_ALL
IdeEnvironment       *ide_config_get_runtime_environment   (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_runtime_environment   (IdeConfig             *self,
                                                            IdeEnvironment        *environment);
IDE_AVAILABLE_IN_ALL
guint                 ide_config_get_sequence              (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_app_id                (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_app_id                (IdeConfig             *self,
                                                            const gchar           *app_id);
IDE_AVAILABLE_IN_ALL
void                  ide_config_apply_path                (IdeConfig             *self,
                                                            IdeSubprocessLauncher *launcher);
IDE_AVAILABLE_IN_ALL
gboolean              ide_config_supports_runtime          (IdeConfig             *self,
                                                            IdeRuntime            *runtime);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_config_get_internal_string       (IdeConfig             *self,
                                                            const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_internal_string       (IdeConfig             *self,
                                                            const gchar           *key,
                                                            const gchar           *value);
IDE_AVAILABLE_IN_ALL
const gchar * const  *ide_config_get_internal_strv         (IdeConfig             *self,
                                                            const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_internal_strv         (IdeConfig             *self,
                                                            const gchar           *key,
                                                            const gchar *const    *value);
IDE_AVAILABLE_IN_ALL
gboolean              ide_config_get_internal_boolean      (IdeConfig             *self,
                                                            const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_internal_boolean      (IdeConfig             *self,
                                                            const gchar           *key,
                                                            gboolean               value);
IDE_AVAILABLE_IN_ALL
gint                  ide_config_get_internal_int          (IdeConfig             *self,
                                                            const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_internal_int          (IdeConfig             *self,
                                                            const gchar           *key,
                                                            gint                   value);
IDE_AVAILABLE_IN_ALL
gint64                ide_config_get_internal_int64        (IdeConfig             *self,
                                                            const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_internal_int64        (IdeConfig             *self,
                                                            const gchar           *key,
                                                            gint64                 value);
IDE_AVAILABLE_IN_ALL
gpointer              ide_config_get_internal_object       (IdeConfig             *self,
                                                            const gchar           *key);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_internal_object       (IdeConfig             *self,
                                                            const gchar           *key,
                                                            gpointer               instance);
IDE_AVAILABLE_IN_ALL
GPtrArray            *ide_config_get_extensions            (IdeConfig             *self);
IDE_AVAILABLE_IN_ALL
const gchar * const  *ide_config_get_args_for_phase        (IdeConfig             *self,
                                                            IdePipelinePhase       phase);
IDE_AVAILABLE_IN_ALL
void                  ide_config_set_args_for_phase        (IdeConfig             *self,
                                                            IdePipelinePhase       phase,
                                                            const gchar *const    *args);
IDE_AVAILABLE_IN_44
GFile                *ide_config_translate_file            (IdeConfig             *self,
                                                            GFile                 *file);
IDE_AVAILABLE_IN_47
void                  ide_config_replace_config_opt        (IdeConfig             *self,
                                                            const char            *param,
                                                            const char            *value);

G_END_DECLS
