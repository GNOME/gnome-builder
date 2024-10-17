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

#include <glib.h>

G_BEGIN_DECLS

typedef struct _IdeBuildLog IdeBuildLog;
typedef struct _IdeBuildManager IdeBuildManager;
typedef struct _IdeBuildSystem IdeBuildSystem;
typedef struct _IdeBuildSystemDiscovery IdeBuildSystemDiscovery;
typedef struct _IdeBuildTarget IdeBuildTarget;
typedef struct _IdeBuildTargetProvider IdeBuildTargetProvider;
typedef struct _IdeCompileCommands IdeCompileCommands;
typedef struct _IdeConfig IdeConfig;
typedef struct _IdeConfigManager IdeConfigManager;
typedef struct _IdeConfigProvider IdeConfigProvider;
typedef struct _IdeDependencyUpdater IdeDependencyUpdater;
typedef struct _IdeDeployStrategy IdeDeployStrategy;
typedef struct _IdeDevice IdeDevice;
typedef struct _IdeDeviceInfo IdeDeviceInfo;
typedef struct _IdeDeviceManager IdeDeviceManager;
typedef struct _IdeDeviceProvider IdeDeviceProvider;
typedef struct _IdeLocalDevice IdeLocalDevice;
typedef struct _IdePipeline IdePipeline;
typedef struct _IdePipelineAddin IdePipelineAddin;
typedef struct _IdePipelineStage IdePipelineStage;
typedef struct _IdePipelineStageLauncher IdePipelineStageLauncher;
typedef struct _IdePipelineStageMkdirs IdePipelineStageMkdirs;
typedef struct _IdePipelineStageTransfer IdePipelineStageTransfer;
typedef struct _IdeRunContext IdeRunContext;
typedef struct _IdeRunCommand IdeRunCommand;
typedef struct _IdeRunCommandProvider IdeRunCommandProvider;
typedef struct _IdeRunManager IdeRunManager;
typedef struct _IdeRuntime IdeRuntime;
typedef struct _IdeRuntimeManager IdeRuntimeManager;
typedef struct _IdeRuntimeProvider IdeRuntimeProvider;
typedef struct _IdeSdk IdeSdk;
typedef struct _IdeSdkProvider IdeSdkProvider;
typedef struct _IdeSdkManager IdeSdkManager;
typedef struct _IdeSimpleBuildTarget IdeSimpleBuildTarget;
typedef struct _IdeSimpleToolchain IdeSimpleToolchain;
typedef struct _IdeTest IdeTest;
typedef struct _IdeTestManager IdeTestManager;
typedef struct _IdeTestProvider IdeTestProvider;
typedef struct _IdeToolchain IdeToolchain;
typedef struct _IdeToolchainManager IdeToolchainManager;
typedef struct _IdeToolchainProvider IdeToolchainProvider;
typedef struct _IdeTriplet IdeTriplet;

G_END_DECLS
