/* ide-foundry-types.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <glib.h>

G_BEGIN_DECLS

typedef struct _IdeBuildLog IdeBuildLog;
typedef struct _IdeBuildManager IdeBuildManager;
typedef struct _IdeBuildPipeline IdeBuildPipeline;
typedef struct _IdeBuildPipelineAddin IdeBuildPipelineAddin;
typedef struct _IdeBuildStage IdeBuildStage;
typedef struct _IdeBuildStageLauncher IdeBuildStageLauncher;
typedef struct _IdeBuildStageMkdirs IdeBuildStageMkdirs;
typedef struct _IdeBuildStageTransfer IdeBuildStageTransfer;
typedef struct _IdeBuildSystem IdeBuildSystem;
typedef struct _IdeBuildSystemDiscovery IdeBuildSystemDiscovery;
typedef struct _IdeBuildTarget IdeBuildTarget;
typedef struct _IdeBuildTargetProvider IdeBuildTargetProvider;
typedef struct _IdeCompileCommands IdeCompileCommands;
typedef struct _IdeConfiguration IdeConfiguration;
typedef struct _IdeConfigurationProvider IdeConfigurationProvider;
typedef struct _IdeConfigurationManager IdeConfigurationManager;
typedef struct _IdeDependencyUpdater IdeDependencyUpdater;
typedef struct _IdeDeployStrategy IdeDeployStrategy;
typedef struct _IdeDevice IdeDevice;
typedef struct _IdeDeviceInfo IdeDeviceInfo;
typedef struct _IdeDeviceManager IdeDeviceManager;
typedef struct _IdeDeviceProvider IdeDeviceProvider;
typedef struct _IdeLocalDevice IdeLocalDevice;
typedef struct _IdeRunButton IdeRunButton;
typedef struct _IdeRunManager IdeRunManager;
typedef struct _IdeRunner IdeRunner;
typedef struct _IdeRunnerAddin IdeRunnerAddin;
typedef struct _IdeRuntime IdeRuntime;
typedef struct _IdeRuntimeManager IdeRuntimeManager;
typedef struct _IdeRuntimeProvider IdeRuntimeProvider;
typedef struct _IdeSimpleBuildTarget IdeSimpleBuildTarget;
typedef struct _IdeSimpleToolchain IdeSimpleToolchain;
typedef struct _IdeTriplet IdeTriplet;
typedef struct _IdeTest IdeTest;
typedef struct _IdeTestManager IdeTestManager;
typedef struct _IdeTestProvider IdeTestProvider;
typedef struct _IdeToolchain IdeToolchain;
typedef struct _IdeToolchainManager IdeToolchainManager;
typedef struct _IdeToolchainProvider IdeToolchainProvider;

G_END_DECLS
