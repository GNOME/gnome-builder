/* libide-lsp.h
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

#define IDE_LSP_INSIDE

#include "ide-lsp-types.h"

#include "ide-lsp-client.h"
#include "ide-lsp-code-action.h"
#include "ide-lsp-code-action-provider.h"
#include "ide-lsp-completion-item.h"
#include "ide-lsp-completion-provider.h"
#include "ide-lsp-completion-results.h"
#include "ide-lsp-diagnostic.h"
#include "ide-lsp-diagnostic-provider.h"
#include "ide-lsp-enums.h"
#include "ide-lsp-formatter.h"
#include "ide-lsp-highlighter.h"
#include "ide-lsp-hover-provider.h"
#include "ide-lsp-plugin.h"
#include "ide-lsp-rename-provider.h"
#include "ide-lsp-search-provider.h"
#include "ide-lsp-service.h"
#include "ide-lsp-symbol-node.h"
#include "ide-lsp-symbol-resolver.h"
#include "ide-lsp-symbol-tree.h"
#include "ide-lsp-util.h"
#include "ide-lsp-workspace-edit.h"

#undef IDE_LSP_INSIDE
