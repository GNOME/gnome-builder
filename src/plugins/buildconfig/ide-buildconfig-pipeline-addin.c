/* ide-buildconfig-pipeline-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-buildconfig-pipeline-addin"

#include "config.h"

#include <libide-foundry.h>
#include <libide-threading.h>

#include "ide-buildconfig-config.h"
#include "ide-buildconfig-pipeline-addin.h"

static void
add_command (IdePipelineAddin   *addin,
             IdePipeline        *pipeline,
             IdePipelinePhase    phase,
             int                 priority,
             const char         *command_text,
             const char * const *environ)
{
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) argv = NULL;
  guint stage_id;
  int argc = 0;

  if (!g_shell_parse_argv (command_text, &argc, &argv, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  run_command = ide_run_command_new ();
  ide_run_command_set_argv (run_command, (const char * const *)argv);
  ide_run_command_set_environ (run_command, environ);

  stage_id = ide_pipeline_attach_command (pipeline, phase, priority, run_command);
  ide_pipeline_addin_track (addin, stage_id);
}

static void
ide_buildconfig_pipeline_addin_load (IdePipelineAddin *addin,
                                     IdePipeline      *pipeline)
{
  const char * const *prebuild;
  const char * const *postbuild;
  IdeConfig *config;
  g_auto(GStrv) env = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDCONFIG_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_PIPELINE (pipeline));

  config = ide_pipeline_get_config (pipeline);

  if (!IDE_IS_BUILDCONFIG_CONFIG (config))
    return;

  env = ide_config_get_environ (config);

  prebuild = ide_buildconfig_config_get_prebuild (IDE_BUILDCONFIG_CONFIG (config));
  postbuild = ide_buildconfig_config_get_postbuild (IDE_BUILDCONFIG_CONFIG (config));

  if (prebuild != NULL)
    {
      for (guint i = 0; prebuild[i]; i++)
        add_command (addin, pipeline, IDE_PIPELINE_PHASE_BUILD|IDE_PIPELINE_PHASE_BEFORE, i, prebuild[i], (const char * const *)env);
    }

  if (postbuild != NULL)
    {
      for (guint i = 0; postbuild[i]; i++)
        add_command (addin, pipeline, IDE_PIPELINE_PHASE_BUILD|IDE_PIPELINE_PHASE_AFTER, i, postbuild[i], (const char * const *)env);
    }

  IDE_EXIT;
}

static void
pipeline_addin_init (IdePipelineAddinInterface *iface)
{
  iface->load = ide_buildconfig_pipeline_addin_load;
}

struct _IdeBuildconfigPipelineAddin { IdeObject parent_instance; };
G_DEFINE_TYPE_EXTENDED (IdeBuildconfigPipelineAddin, ide_buildconfig_pipeline_addin, IDE_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_init))
static void ide_buildconfig_pipeline_addin_class_init (IdeBuildconfigPipelineAddinClass *klass) { }
static void ide_buildconfig_pipeline_addin_init (IdeBuildconfigPipelineAddin *self) { }
