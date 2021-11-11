/* libide-code.h
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

#define IDE_CODE_INSIDE

#include "ide-code-enums.h"
#include "ide-code-types.h"

#include "ide-buffer.h"
#include "ide-buffer-addin.h"
#include "ide-buffer-change-monitor.h"
#include "ide-buffer-manager.h"
#include "ide-code-action.h"
#include "ide-code-action-provider.h"
#include "ide-code-index-entries.h"
#include "ide-code-index-entry.h"
#include "ide-code-indexer.h"
#include "ide-diagnostic.h"
#include "ide-diagnostic-provider.h"
#include "ide-diagnostics.h"
#include "ide-diagnostics-manager.h"
#include "ide-file-settings.h"
#include "ide-formatter-options.h"
#include "ide-formatter.h"
#include "ide-highlight-engine.h"
#include "ide-highlight-index.h"
#include "ide-highlighter.h"
#include "ide-indent-style.h"
#include "ide-language.h"
#include "ide-location.h"
#include "ide-range.h"
#include "ide-rename-provider.h"
#include "ide-source-iter.h"
#include "ide-source-style-scheme.h"
#include "ide-spaces-style.h"
#include "ide-symbol-node.h"
#include "ide-symbol-resolver.h"
#include "ide-symbol-tree.h"
#include "ide-symbol.h"
#include "ide-text-edit.h"
#include "ide-text-iter.h"
#include "ide-unsaved-file.h"
#include "ide-unsaved-files.h"

#undef IDE_CODE_INSIDE

G_END_DECLS
