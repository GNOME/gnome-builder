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

G_DECLARE_DERIVABLE_TYPE (IdeConfiguration, ide_configuration, IDE, CONFIGURATION, IdeObject)

struct _IdeConfigurationClass
{
  IdeObjectClass parent;

  IdeDevice  *(*get_device)       (IdeConfiguration *self);
  void        (*set_device)       (IdeConfiguration *self,
                                   IdeDevice        *device);

  IdeRuntime *(*get_runtime)      (IdeConfiguration *self);
  void        (*set_runtime)      (IdeConfiguration *self,
                                   IdeRuntime       *runtime);

  gboolean    (*supports_device)  (IdeConfiguration *self,
                                   IdeDevice        *device);
  gboolean    (*supports_runtime) (IdeConfiguration *self,
                                   IdeRuntime       *runtime);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
  gpointer _reserved9;
  gpointer _reserved10;
  gpointer _reserved11;
  gpointer _reserved12;
  gpointer _reserved13;
  gpointer _reserved14;
  gpointer _reserved15;
  gpointer _reserved16;
};

IdeConfiguration     *ide_configuration_new                       (IdeContext            *context,
                                                                   const gchar           *id,
                                                                   const gchar           *device_id,
                                                                   const gchar           *runtime_id);
const gchar          *ide_configuration_get_id                    (IdeConfiguration      *self);
const gchar          *ide_configuration_get_runtime_id            (IdeConfiguration      *self);
void                  ide_configuration_set_runtime_id            (IdeConfiguration      *self,
                                                                   const gchar           *runtime_id);
const gchar          *ide_configuration_get_device_id             (IdeConfiguration      *self);
void                  ide_configuration_set_device_id             (IdeConfiguration      *self,
                                                                   const gchar           *device_id);
IdeDevice            *ide_configuration_get_device                (IdeConfiguration      *self);
void                  ide_configuration_set_device                (IdeConfiguration      *self,
                                                                   IdeDevice             *device);
gboolean              ide_configuration_get_dirty                 (IdeConfiguration      *self);
void                  ide_configuration_set_dirty                 (IdeConfiguration      *self,
                                                                   gboolean               dirty);
const gchar          *ide_configuration_get_display_name          (IdeConfiguration      *self);
void                  ide_configuration_set_display_name          (IdeConfiguration      *self,
                                                                   const gchar           *display_name);
gboolean              ide_configuration_get_ready                 (IdeConfiguration      *self);
IdeRuntime           *ide_configuration_get_runtime               (IdeConfiguration      *self);
void                  ide_configuration_set_runtime               (IdeConfiguration      *self,
                                                                   IdeRuntime            *runtime);
gchar               **ide_configuration_get_environ               (IdeConfiguration      *self);
const gchar          *ide_configuration_getenv                    (IdeConfiguration      *self,
                                                                   const gchar           *key);
void                  ide_configuration_setenv                    (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   const gchar           *value);
gboolean              ide_configuration_get_debug                 (IdeConfiguration      *self);
void                  ide_configuration_set_debug                 (IdeConfiguration      *self,
                                                                   gboolean               debug);
const gchar          *ide_configuration_get_prefix                (IdeConfiguration      *self);
void                  ide_configuration_set_prefix                (IdeConfiguration      *self,
                                                                   const gchar           *prefix);
const gchar          *ide_configuration_get_config_opts           (IdeConfiguration      *self);
void                  ide_configuration_set_config_opts           (IdeConfiguration      *self,
                                                                   const gchar           *config_opts);
const gchar          *ide_configuration_get_run_opts              (IdeConfiguration      *self);
void                  ide_configuration_set_run_opts              (IdeConfiguration      *self,
                                                                   const gchar           *run_opts);
const gchar * const  *ide_configuration_get_build_commands        (IdeConfiguration      *self);
void                  ide_configuration_set_build_commands        (IdeConfiguration      *self,
                                                                   const gchar *const    *build_commands);
const gchar * const  *ide_configuration_get_post_install_commands (IdeConfiguration      *self);
void                  ide_configuration_set_post_install_commands (IdeConfiguration      *self,
                                                                   const gchar *const    *post_install_commands);
gint                  ide_configuration_get_parallelism           (IdeConfiguration      *self);
void                  ide_configuration_set_parallelism           (IdeConfiguration      *self,
                                                                   gint                   parallelism);
IdeEnvironment       *ide_configuration_get_environment           (IdeConfiguration      *self);
void                  ide_configuration_set_environment           (IdeConfiguration      *self,
                                                                   IdeEnvironment        *environment);
IdeConfiguration     *ide_configuration_duplicate                 (IdeConfiguration      *self);
IdeConfiguration     *ide_configuration_snapshot                  (IdeConfiguration      *self);
guint                 ide_configuration_get_sequence              (IdeConfiguration      *self);
const gchar          *ide_configuration_get_app_id                (IdeConfiguration      *self);
void                  ide_configuration_set_app_id                (IdeConfiguration      *self,
                                                                   const gchar           *app_id);
gboolean              ide_configuration_supports_device           (IdeConfiguration      *self,
                                                                   IdeDevice             *device);
gboolean              ide_configuration_supports_runtime          (IdeConfiguration      *self,
                                                                   IdeRuntime            *runtime);
const gchar          *ide_configuration_get_internal_string       (IdeConfiguration      *self,
                                                                   const gchar           *key);
void                  ide_configuration_set_internal_string       (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   const gchar           *value);
const gchar * const  *ide_configuration_get_internal_strv         (IdeConfiguration      *self,
                                                                   const gchar           *key);
void                  ide_configuration_set_internal_strv         (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   const gchar *const    *value);
gboolean              ide_configuration_get_internal_boolean      (IdeConfiguration      *self,
                                                                   const gchar           *key);
void                  ide_configuration_set_internal_boolean      (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gboolean               value);
gint                  ide_configuration_get_internal_int          (IdeConfiguration      *self,
                                                                   const gchar           *key);
void                  ide_configuration_set_internal_int          (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gint                   value);
gint64                ide_configuration_get_internal_int64        (IdeConfiguration      *self,
                                                                   const gchar           *key);
void                  ide_configuration_set_internal_int64        (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gint64                 value);
gpointer              ide_configuration_get_internal_object       (IdeConfiguration      *self,
                                                                   const gchar           *key);
void                  ide_configuration_set_internal_object       (IdeConfiguration      *self,
                                                                   const gchar           *key,
                                                                   gpointer               instance);
void                  _ide_configuration_set_prebuild             (IdeConfiguration      *self,
                                                                   IdeBuildCommandQueue  *prebuild) G_GNUC_INTERNAL;
void                  _ide_configuration_set_postbuild            (IdeConfiguration      *self,
                                                                   IdeBuildCommandQueue  *postbuild) G_GNUC_INTERNAL;

G_END_DECLS

#endif /* IDE_CONFIGURATION_H */
