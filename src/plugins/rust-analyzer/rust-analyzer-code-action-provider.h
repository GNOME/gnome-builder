/* rust-analyzer-code-action-provider.h
 *
 * Copyright 2021 Georg Vienna <georg.vienna@himbarsoft.com>
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

#include <libide-lsp.h>

G_BEGIN_DECLS

#define RUST_TYPE_ANALYZER_CODE_ACTION_PROVIDER (rust_analyzer_code_action_provider_get_type())

G_DECLARE_FINAL_TYPE (RustAnalyzerCodeActionProvider, rust_analyzer_code_action_provider, RUST, ANALYZER_CODE_ACTION_PROVIDER, IdeLspCodeActionProvider)

G_END_DECLS
