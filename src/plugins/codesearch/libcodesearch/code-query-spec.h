/*
 * code-query-spec.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define CODE_TYPE_QUERY_SPEC (code_query_spec_get_type())

G_DECLARE_FINAL_TYPE (CodeQuerySpec, code_query_spec, CODE, QUERY_SPEC, GObject)

CodeQuerySpec *code_query_spec_new_for_regex  (GRegex     *regex);
CodeQuerySpec *code_query_spec_new_contains   (const char *string);

G_END_DECLS
