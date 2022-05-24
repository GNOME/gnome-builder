/* gbp-gopls-highlighter.c
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

#define G_LOG_DOMAIN "gbp-gopls-highlighter"

#include "config.h"

#include "gbp-gopls-highlighter.h"
#include "gbp-gopls-service.h"

struct _GbpGoplsHighlighter
{
  IdeLspHighlighter parent_instance;
};

static void
gbp_gopls_highlighter_load (IdeHighlighter *provider)
{
  g_autoptr(IdeLspServiceClass) klass = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GOPLS_HIGHLIGHTER (provider));

  klass = g_type_class_ref (GBP_TYPE_GOPLS_SERVICE);
  ide_lsp_service_class_bind_client (klass, IDE_OBJECT (provider));

  IDE_EXIT;
}

static void
highlighter_iface_init (IdeHighlighterInterface *iface)
{
  iface->load = gbp_gopls_highlighter_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGoplsHighlighter, gbp_gopls_highlighter, IDE_TYPE_LSP_HIGHLIGHTER,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_HIGHLIGHTER, highlighter_iface_init))

static void
gbp_gopls_highlighter_class_init (GbpGoplsHighlighterClass *klass)
{
}

static void
gbp_gopls_highlighter_init (GbpGoplsHighlighter *self)
{
}
