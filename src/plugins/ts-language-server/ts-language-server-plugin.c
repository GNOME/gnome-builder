/* ts-language-server-plugin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ts-language-server-plugin"

#include "config.h"

#include <libpeas/peas.h>

#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-lsp.h>

#include "gbp-tslsp-service.h"
#include "gbp-tslsp-completion-provider.h"
#include "gbp-tslsp-diagnostic-provider.h"
#include "gbp-tslsp-symbol-resolver.h"
#include "gbp-tslsp-highlighter.h"
#include "gbp-tslsp-formatter.h"
#include "gbp-tslsp-rename-provider.h"
#include "gbp-tslsp-hover-provider.h"
#include "gbp-tslsp-code-action-provider.h"

_IDE_EXTERN void
_gbp_tslsp_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                              GBP_TYPE_TSLSP_DIAGNOSTIC_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                              GBP_TYPE_TSLSP_COMPLETION_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SYMBOL_RESOLVER,
                                              GBP_TYPE_TSLSP_SYMBOL_RESOLVER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_HIGHLIGHTER,
                                              GBP_TYPE_TSLSP_HIGHLIGHTER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_FORMATTER,
                                              GBP_TYPE_TSLSP_FORMATTER);
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_HOVER_PROVIDER,
                                              GBP_TYPE_TSLSP_HOVER_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RENAME_PROVIDER,
                                              GBP_TYPE_TSLSP_RENAME_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_CODE_ACTION_PROVIDER,
                                              GBP_TYPE_TSLSP_CODE_ACTION_PROVIDER);
}
