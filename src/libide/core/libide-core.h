/* ide-core.h
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

#include <gio/gio.h>
#include <libdex.h>

#define IDE_CORE_INSIDE

#include "ide-action-group.h"
#include "ide-action-muxer.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-global.h"
#include "ide-gsettings-action-group.h"
#include "ide-log.h"
#include "ide-log-item.h"
#include "ide-macros.h"
#include "ide-notification.h"
#include "ide-notifications.h"
#include "ide-object.h"
#include "ide-object-box.h"
#include "ide-property-action-group.h"
#include "ide-settings.h"
#include "ide-settings-flag-action.h"
#include "ide-transfer.h"
#include "ide-transfer-manager.h"
#include "ide-version.h"
#include "ide-version-macros.h"

#undef IDE_CORE_INSIDE
