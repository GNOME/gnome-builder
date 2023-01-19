/* code-query-spec-private.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "code-query-spec.h"
#include "code-sparse-set.h"

G_BEGIN_DECLS

gboolean _code_query_spec_matches          (CodeQuerySpec *spec,
                                            const char    *path,
                                            GBytes        *bytes);
void     _code_query_spec_collect_trigrams (CodeQuerySpec *spec,
                                            CodeSparseSet *set);

G_END_DECLS
