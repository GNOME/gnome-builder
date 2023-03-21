/*
 * swiftlint-plugin.c
 *
 * Copyright 2023 JCWasmx86 <JCWasmx86@t-online.de>
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

#define G_LOG_DOMAIN "swiftlint-plugin"

#include "config.h"

#include <libpeas.h>

#include <libide-code.h>

#include "gbp-swiftlint-diagnostic-provider.h"

_IDE_EXTERN void
_gbp_swiftlint_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                              GBP_TYPE_SWIFTLINT_DIAGNOSTIC_PROVIDER);
}
