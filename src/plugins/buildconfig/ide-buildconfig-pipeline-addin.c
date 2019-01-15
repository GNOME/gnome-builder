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
add_command (IdePipelineAddin  *addin,
             IdePipeline       *pipeline,
             IdePipelinePhase           phase,
             gint                    priority,
             const gchar            *command_text,
             gchar                 **env)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_auto(GStrv) argv = NULL;
  guint stage_id;
  gint argc = 0;

  if (!g_shell_parse_argv (command_text, &argc, &argv, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  launcher = ide_pipeline_create_launcher (pipeline, NULL);

  if (launcher == NULL)
    {
      g_warning ("Failed to create launcher for build command");
      return;
    }

  for (guint i = 0; i < argc; i++)
    ide_subprocess_launcher_push_argv (launcher, argv[i]);

  ide_subprocess_launcher_set_environ (launcher, (const gchar * const *)env);

  stage_id = ide_pipeline_attach_launcher (pipeline, phase, priority, launcher);
  ide_pipeline_addin_track (addin, stage_id);
}

static void
ide_buildconfig_pipeline_addin_load (IdePipelineAddin *addin,
                                     IdePipeline      *pipeline)
{
  const gchar * const *prebuild;
  const gchar * const *postbuild;
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
        add_command (addin, pipeline, IDE_PIPELINE_PHASE_BUILD|IDE_PIPELINE_PHASE_BEFORE, i, prebuild[i], env);
    }

  if (postbuild != NULL)
    {
      for (guint i = 0; postbuild[i]; i++)
        add_command (addin, pipeline, IDE_PIPELINE_PHASE_BUILD|IDE_PIPELINE_PHASE_AFTER, i, postbuild[i], env);
    }

  IDE_EXIT;
}

static void
pipeline_addin_init (IdePipelineAddinInterface *iface)
{
  iface->load = ide_buildconfig_pipeline_addin_load;
}

struct _IdeBuildconfigPipelineAddin { IdeObject parent_instance; };
G_DEFINE_TYPE_EXTENDED (IdeBuildconfigPipelineAddin, ide_buildconfig_pipeline_addin, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_init))
static void ide_buildconfig_pipeline_addin_class_init (IdeBuildconfigPipelineAddinClass *klass) { }
static void ide_buildconfig_pipeline_addin_init (IdeBuildconfigPipelineAddin *self) { }
