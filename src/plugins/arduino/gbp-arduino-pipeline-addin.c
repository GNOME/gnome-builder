/*
 * gbp-arduino-pipeline-addin.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-arduino-pipeline-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-foundry.h>

#include "gbp-arduino-build-system.h"
#include "gbp-arduino-pipeline-addin.h"
#include "gbp-arduino-port.h"
#include "gbp-arduino-profile.h"

struct _GbpArduinoPipelineAddin
{
  IdeObject parent_instance;
  guint     error_format_id;
};

static IdeRunCommand *
create_update_command (GbpArduinoPipelineAddin *addin,
                       IdePipeline             *pipeline,
                       const char              *project_dir,
                       const char              *arduino_path,
                       IdeConfig               *config)
{
  g_autoptr (IdeRunCommand) command = NULL;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_CONFIG (config));

  command = ide_run_command_new ();
  ide_run_command_set_cwd (command, project_dir);

  ide_run_command_append_argv (command, arduino_path);

  ide_run_command_append_argv (command, "core");
  ide_run_command_append_argv (command, "upgrade");

  return g_steal_pointer (&command);
}

static IdeRunCommand *
create_compile_command (GbpArduinoPipelineAddin *self,
                        IdePipeline             *pipeline,
                        const char              *project_dir,
                        const char              *arduino_path,
                        IdeConfig               *config)
{
  GbpArduinoProfile *arduino_profile = (GbpArduinoProfile *) config;
  g_autoptr (IdeRunCommand) command = NULL;
  const char *build_directory = NULL;
  const char *profile_name = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_CONFIG (config));

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  build_directory = ide_build_system_get_builddir (build_system, pipeline);
  profile_name = ide_config_get_id (IDE_CONFIG (arduino_profile));

  command = ide_run_command_new ();
  ide_run_command_set_cwd (command, project_dir);

  ide_run_command_append_argv (command, arduino_path);
  ide_run_command_append_argv (command, "compile");

  /* Added a port otherwise it doesn't compile if a valid port is set in sketch.yaml */
  ide_run_command_append_argv (command, "--port");
  ide_run_command_append_argv (command, "X");

  ide_run_command_append_argv (command, "--profile");
  ide_run_command_append_argv (command, profile_name);

  ide_run_command_append_argv (command, "--build-path");
  ide_run_command_append_argv (command, build_directory);

  return g_steal_pointer (&command);
}

static IdeRunCommand *
create_upload_command (GbpArduinoPipelineAddin *self,
                       IdePipeline             *pipeline,
                       const char              *project_dir,
                       const char              *arduino_path,
                       IdeConfig               *config)
{
  g_autoptr (IdeRunCommand) command = NULL;
  const char *port_address = NULL;
  const char *profile_name = NULL;
  const char *build_directory = NULL;
  GbpArduinoProfile *arduino_profile;
  IdeBuildSystem *build_system;
  IdeContext *context;
  IdeDevice *device;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_CONFIG (config));

  arduino_profile = GBP_ARDUINO_PROFILE (config);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  device = ide_pipeline_get_device (pipeline);

  if (!GBP_IS_ARDUINO_PORT (device))
    {
      return NULL;
    }

  port_address = gbp_arduino_port_get_address (GBP_ARDUINO_PORT (device));
  profile_name = ide_config_get_id (IDE_CONFIG (arduino_profile));
  build_directory = ide_build_system_get_builddir (build_system, pipeline);

  command = ide_run_command_new ();
  ide_run_command_set_cwd (command, project_dir);

  ide_run_command_append_argv (command, arduino_path);
  ide_run_command_append_argv (command, "upload");

  ide_run_command_append_argv (command, "--port");
  ide_run_command_append_argv (command, port_address);

  ide_run_command_append_argv (command, "--profile");
  ide_run_command_append_argv (command, profile_name);

  ide_run_command_append_argv (command, "--build-path");
  ide_run_command_append_argv (command, build_directory);

  ide_run_command_append_argv (command, "--verbose");

  return g_steal_pointer (&command);
}

static void
compile_query_cb (IdePipelineStage *stage,
                  IdePipeline      *pipeline,
                  GPtrArray        *targets,
                  GCancellable     *cancellable,
                  gpointer          user_data)
{
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_pipeline_stage_set_completed (stage, FALSE);
}

static void
gbp_arduino_pipeline_addin_load (IdePipelineAddin *addin,
                                 IdePipeline      *pipeline)
{
  GbpArduinoPipelineAddin *self = (GbpArduinoPipelineAddin *) addin;
  g_autoptr (IdeRunCommand) compile_command = NULL;
  g_autoptr (IdeRunCommand) upload_command = NULL;
  g_autoptr (IdeRunCommand) update_command = NULL;
  g_autoptr (IdePipelineStage) update_stage = NULL;
  g_autoptr (IdePipelineStage) compile_stage = NULL;
  g_autoptr (IdePipelineStage) upload_stage = NULL;
  g_autofree char *project_dir = NULL;
  g_autofree char *arduino_path = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;
  IdeConfig *config;
  guint stage_id;

  g_assert (GBP_IS_ARDUINO_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_ARDUINO_BUILD_SYSTEM (build_system))
    return;

  project_dir = gbp_arduino_build_system_get_project_dir (GBP_ARDUINO_BUILD_SYSTEM (build_system));
  config = ide_pipeline_get_config (pipeline);
  arduino_path = gbp_arduino_build_system_locate_arduino (GBP_ARDUINO_BUILD_SYSTEM (build_system));

  g_assert (project_dir != NULL);
  g_assert (IDE_IS_CONFIG (config));
  g_assert (arduino_path != NULL);

  self->error_format_id = ide_pipeline_add_error_format (pipeline,
                                                         "(?<filename>[a-zA-Z0-9\\-\\.\\/_]+\\.ino):"
                                                         "(?<line>\\d+):"
                                                         "(?<column>\\d+): "
                                                         ".+(?<level>(?:error|warning)): "
                                                         "(?<message>.*)",
                                                         G_REGEX_OPTIMIZE);

  /* Update stage */
  update_command = create_update_command (self, pipeline, project_dir, arduino_path, config);
  update_stage = ide_pipeline_stage_command_new (update_command, NULL);
  ide_pipeline_stage_set_name (update_stage, _("Update Arduino Packages"));
  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_DEPENDENCIES, 200, update_stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  /* Compilation stage */
  compile_command = create_compile_command (self, pipeline, project_dir, arduino_path, config);
  compile_stage = ide_pipeline_stage_command_new (compile_command, NULL);
  ide_pipeline_stage_set_name (compile_stage, _("Compile Arduino Sketch"));
  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_BUILD, 200, compile_stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);

  g_signal_connect (compile_stage, "query", G_CALLBACK (compile_query_cb), NULL);

  /* Upload stage */
  if (!GBP_IS_ARDUINO_PORT (ide_pipeline_get_device (pipeline)))
    {
      upload_stage = ide_pipeline_stage_command_new (NULL, NULL);
      ide_pipeline_stage_set_disabled (upload_stage, TRUE);
    }
  else
    {
      upload_command = create_upload_command (self, pipeline, project_dir, arduino_path, config);
      upload_stage = ide_pipeline_stage_command_new (upload_command, NULL);
    }

  ide_pipeline_stage_set_name (upload_stage, _("Upload Arduino Sketch"));
  stage_id = ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_INSTALL, 200, upload_stage);
  ide_pipeline_addin_track (IDE_PIPELINE_ADDIN (self), stage_id);
}

static void
gbp_arduino_pipeline_addin_unload (IdePipelineAddin *addin,
                                   IdePipeline      *pipeline)
{
  GbpArduinoPipelineAddin *self = (GbpArduinoPipelineAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_ARDUINO_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (self->error_format_id != 0)
    {
      ide_pipeline_remove_error_format (pipeline, self->error_format_id);
      self->error_format_id = 0;
    }

  IDE_EXIT;
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_arduino_pipeline_addin_load;
  iface->unload = gbp_arduino_pipeline_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpArduinoPipelineAddin, gbp_arduino_pipeline_addin, IDE_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_arduino_pipeline_addin_class_init (GbpArduinoPipelineAddinClass *klass)
{
}

static void
gbp_arduino_pipeline_addin_init (GbpArduinoPipelineAddin *self)
{
}

