/* gbp-intelephense-formatter.c
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

#define G_LOG_DOMAIN "gbp-intelephense-formatter"

#include "config.h"

#include "gbp-intelephense-formatter.h"
#include "gbp-intelephense-service.h"

struct _GbpIntelephenseFormatter
{
  IdeLspFormatter parent_instance;
};

static void
gbp_intelephense_formatter_load (IdeFormatter *provider)
{
  g_autoptr(IdeLspServiceClass) klass = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_INTELEPHENSE_FORMATTER (provider));

  klass = g_type_class_ref (GBP_TYPE_INTELEPHENSE_SERVICE);
  ide_lsp_service_class_bind_client (klass, IDE_OBJECT (provider));

  IDE_EXIT;
}

static void
formatter_iface_init (IdeFormatterInterface *iface)
{
  iface->load = gbp_intelephense_formatter_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpIntelephenseFormatter, gbp_intelephense_formatter, IDE_TYPE_LSP_FORMATTER,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_FORMATTER, formatter_iface_init))

static void
gbp_intelephense_formatter_class_init (GbpIntelephenseFormatterClass *klass)
{
}

static void
gbp_intelephense_formatter_init (GbpIntelephenseFormatter *self)
{
}
