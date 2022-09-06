/* rust-analyzer-plugin.c
 *
 * Copyright 2020-2021 Günther Wagner <info@gunibert.de>
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

#include "config.h"

#include <libpeas/peas.h>
#include <libide-lsp.h>
#include <libide-gui.h>

#include "gbp-rust-analyzer-code-action-provider.h"
#include "gbp-rust-analyzer-completion-provider.h"
#include "gbp-rust-analyzer-diagnostic-provider.h"
#include "gbp-rust-analyzer-formatter.h"
#include "gbp-rust-analyzer-highlighter.h"
#include "gbp-rust-analyzer-hover-provider.h"
#include "gbp-rust-analyzer-rename-provider.h"
#include "gbp-rust-analyzer-search-provider.h"
#include "gbp-rust-analyzer-service.h"
#include "gbp-rust-analyzer-symbol-resolver.h"
#include "gbp-rust-analyzer-tweaks-addin.h"

_IDE_EXTERN void
_gbp_rust_analyzer_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                              GBP_TYPE_RUST_ANALYZER_COMPLETION_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SYMBOL_RESOLVER,
                                              GBP_TYPE_RUST_ANALYZER_SYMBOL_RESOLVER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                              GBP_TYPE_RUST_ANALYZER_DIAGNOSTIC_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_FORMATTER,
                                              GBP_TYPE_RUST_ANALYZER_FORMATTER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_HIGHLIGHTER,
                                              GBP_TYPE_RUST_ANALYZER_HIGHLIGHTER);
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_HOVER_PROVIDER,
                                              GBP_TYPE_RUST_ANALYZER_HOVER_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RENAME_PROVIDER,
                                              GBP_TYPE_RUST_ANALYZER_RENAME_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SEARCH_PROVIDER,
                                              GBP_TYPE_RUST_ANALYZER_SEARCH_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_CODE_ACTION_PROVIDER,
                                              GBP_TYPE_RUST_ANALYZER_CODE_ACTION_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TWEAKS_ADDIN,
                                              GBP_TYPE_RUST_ANALYZER_TWEAKS_ADDIN);
}
