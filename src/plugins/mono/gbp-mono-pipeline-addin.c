/* gbp-mono-pipeline-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-mono-pipeline-addin"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-mono-pipeline-addin.h"

struct _GbpMonoPipelineAddin
{
  IdeObject parent_instance;
  guint     error_format_id;
};

static void
gbp_mono_pipeline_addin_load (IdePipelineAddin *addin,
                              IdePipeline      *pipeline)
{
  GbpMonoPipelineAddin *self = (GbpMonoPipelineAddin *)addin;

  g_assert (IDE_IS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  self->error_format_id = ide_pipeline_add_error_format (pipeline,
                                                         "(?<filename>[a-zA-Z0-9\\-\\.\\/_]+.cs)"
                                                         "\\((?<line>\\d+),(?<column>\\d+)\\): "
                                                         "(?<level>[\\w\\s]+) "
                                                         "(?<code>CS[0-9]+): "
                                                         "(?<message>.*)",
                                                         G_REGEX_OPTIMIZE);
}

static void
gbp_mono_pipeline_addin_unload (IdePipelineAddin *addin,
                                IdePipeline      *pipeline)
{
  GbpMonoPipelineAddin *self = (GbpMonoPipelineAddin *)addin;

  g_assert (IDE_IS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  ide_pipeline_remove_error_format (pipeline, self->error_format_id);
}

static void
pipeline_addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_mono_pipeline_addin_load;
  iface->unload = gbp_mono_pipeline_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMonoPipelineAddin, gbp_mono_pipeline_addin, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, pipeline_addin_iface_init))

static void
gbp_mono_pipeline_addin_class_init (GbpMonoPipelineAddinClass *klass)
{
}

static void
gbp_mono_pipeline_addin_init (GbpMonoPipelineAddin *self)
{
}
