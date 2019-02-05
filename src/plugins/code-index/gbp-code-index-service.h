/* gbp-code-index-service.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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
#include <libide-code.h>

#include "ide-code-index-index.h"

G_BEGIN_DECLS

#define GBP_TYPE_CODE_INDEX_SERVICE (gbp_code_index_service_get_type())

G_DECLARE_FINAL_TYPE (GbpCodeIndexService, gbp_code_index_service, GBP, CODE_INDEX_SERVICE, IdeObject)

GbpCodeIndexService *gbp_code_index_service_from_context (IdeContext          *context);
void                 gbp_code_index_service_start        (GbpCodeIndexService *self);
void                 gbp_code_index_service_stop         (GbpCodeIndexService *self);
gboolean             gbp_code_index_service_get_paused   (GbpCodeIndexService *self);
void                 gbp_code_index_service_set_paused   (GbpCodeIndexService *self,
                                                          gboolean             paused);
IdeCodeIndexIndex   *gbp_code_index_service_get_index    (GbpCodeIndexService *self);
IdeCodeIndexer      *gbp_code_index_service_get_indexer  (GbpCodeIndexService *self,
                                                          const gchar         *lang_id,
                                                          const gchar         *path);

G_END_DECLS
