/*
 * gbp-swiftformat-formatter.h
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
#pragma once

#include <libide-code.h>

G_BEGIN_DECLS

#define GBP_TYPE_SWIFTFORMAT_FORMATTER (gbp_swiftformat_formatter_get_type())

G_DECLARE_FINAL_TYPE (GbpSwiftformatFormatter, gbp_swiftformat_formatter, GBP, SWIFTFORMAT_FORMATTER, IdeObject)

G_END_DECLS
