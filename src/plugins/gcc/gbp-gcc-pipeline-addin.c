/* gbp-gcc-pipeline-addin.c
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

#define G_LOG_DOMAIN "gbp-gcc-pipeline-addin"

#include "config.h"

#include "libide-foundry.h"

#include "gbp-gcc-pipeline-addin.h"

#define ERROR_FORMAT_REGEX                  \
  "(?<filename>[a-zA-Z0-9\\+\\-\\.\\/_]+):" \
  "(?<line>\\d+):"                          \
  "(?<column>\\d+): "                       \
  "(?<level>[\\w\\s]+): "                   \
  "(?<message>.*)"

struct _GbpGccPipelineAddin
{
  IdeObject parent_instance;
  guint     error_format_id;
};

static void
gbp_gcc_pipeline_addin_load (IdePipelineAddin *addin,
                             IdePipeline      *pipeline)
{
  GbpGccPipelineAddin *self = (GbpGccPipelineAddin *)addin;

  g_assert (GBP_IS_GCC_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  self->error_format_id = ide_pipeline_add_error_format (pipeline,
                                                               ERROR_FORMAT_REGEX,
                                                               G_REGEX_CASELESS);
}

static void
gbp_gcc_pipeline_addin_unload (IdePipelineAddin *addin,
                               IdePipeline      *pipeline)
{
  GbpGccPipelineAddin *self = (GbpGccPipelineAddin *)addin;

  g_assert (GBP_IS_GCC_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  ide_pipeline_remove_error_format (pipeline, self->error_format_id);
  self->error_format_id = 0;
}

static void
addin_iface_init (IdePipelineAddinInterface *iface)
{
  iface->load = gbp_gcc_pipeline_addin_load;
  iface->unload = gbp_gcc_pipeline_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGccPipelineAddin, gbp_gcc_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PIPELINE_ADDIN, addin_iface_init))

static void gbp_gcc_pipeline_addin_class_init (GbpGccPipelineAddinClass *klass) { }
static void gbp_gcc_pipeline_addin_init (GbpGccPipelineAddin *self) { }
