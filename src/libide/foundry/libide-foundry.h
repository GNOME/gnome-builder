/* libide-foundry.h"
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

#include <libide-core.h>
#include <libide-io.h>
#include <libide-threading.h>

G_BEGIN_DECLS

#define IDE_FOUNDRY_INSIDE

#include "ide-build-log.h"
#include "ide-build-manager.h"
#include "ide-build-system-discovery.h"
#include "ide-build-system.h"
#include "ide-build-target-provider.h"
#include "ide-build-target.h"
#include "ide-compile-commands.h"
#include "ide-config-manager.h"
#include "ide-config-provider.h"
#include "ide-config.h"
#include "ide-dependency-updater.h"
#include "ide-deploy-strategy.h"
#include "ide-device-info.h"
#include "ide-device-manager.h"
#include "ide-device-provider.h"
#include "ide-device.h"
#include "ide-diagnostic-tool.h"
#include "ide-fallback-build-system.h"
#include "ide-foundry-compat.h"
#include "ide-foundry-global.h"
#include "ide-local-device.h"
#include "ide-path-cache.h"
#include "ide-pipeline-addin.h"
#include "ide-pipeline-stage-command.h"
#include "ide-pipeline-stage-launcher.h"
#include "ide-pipeline-stage-mkdirs.h"
#include "ide-pipeline-stage-transfer.h"
#include "ide-pipeline-stage.h"
#include "ide-pipeline.h"
#include "ide-pty.h"
#include "ide-run-command.h"
#include "ide-run-command-provider.h"
#include "ide-run-commands.h"
#include "ide-run-context.h"
#include "ide-run-manager.h"
#include "ide-run-tool.h"
#include "ide-runtime-manager.h"
#include "ide-runtime-provider.h"
#include "ide-runtime.h"
#include "ide-sdk.h"
#include "ide-sdk-manager.h"
#include "ide-sdk-provider.h"
#include "ide-simple-build-system-discovery.h"
#include "ide-simple-build-target.h"
#include "ide-simple-toolchain.h"
#include "ide-test-manager.h"
#include "ide-test.h"
#include "ide-toolchain-manager.h"
#include "ide-toolchain-provider.h"
#include "ide-toolchain.h"
#include "ide-triplet.h"

#undef IDE_FOUNDRY_INSIDE

G_END_DECLS
