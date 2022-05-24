/* gbp-gjs-code-indexer.c
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

#define G_LOG_DOMAIN "gbp-gjs-code-indexer"

#include "config.h"

#include <libide-code.h>

#include "gbp-gjs-code-indexer.h"

struct _GbpGjsCodeIndexer
{
  IdeObject parent_instance;
};

static void
code_indexer_iface_init (IdeCodeIndexerInterface *iface)
{
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGjsCodeIndexer, gbp_gjs_code_indexer, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_INDEXER, code_indexer_iface_init))

static void
gbp_gjs_code_indexer_class_init (GbpGjsCodeIndexerClass *klass)
{
}

static void
gbp_gjs_code_indexer_init (GbpGjsCodeIndexer *self)
{
}
