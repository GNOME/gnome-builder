/* ide-pipeline-phase.h
 *
 * Copyright 2021 Günther Wagner <info@gunibert.de>
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

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  IDE_PIPELINE_PHASE_NONE         = 0,
  IDE_PIPELINE_PHASE_PREPARE      = 1 << 0,
  IDE_PIPELINE_PHASE_DOWNLOADS    = 1 << 1,
  IDE_PIPELINE_PHASE_DEPENDENCIES = 1 << 2,
  IDE_PIPELINE_PHASE_AUTOGEN      = 1 << 3,
  IDE_PIPELINE_PHASE_CONFIGURE    = 1 << 4,
  IDE_PIPELINE_PHASE_BUILD        = 1 << 6,
  IDE_PIPELINE_PHASE_INSTALL      = 1 << 7,
  IDE_PIPELINE_PHASE_COMMIT       = 1 << 8,
  IDE_PIPELINE_PHASE_EXPORT       = 1 << 9,
  IDE_PIPELINE_PHASE_FINAL        = 1 << 10,
  IDE_PIPELINE_PHASE_BEFORE       = 1 << 28,
  IDE_PIPELINE_PHASE_AFTER        = 1 << 29,
  IDE_PIPELINE_PHASE_FINISHED     = 1 << 30,
  IDE_PIPELINE_PHASE_FAILED       = 1 << 31,
} IdePipelinePhase;

G_END_DECLS

