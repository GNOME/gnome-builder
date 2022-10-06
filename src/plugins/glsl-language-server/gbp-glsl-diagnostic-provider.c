/* gbp-glsl-diagnostic-provider.c
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

#define G_LOG_DOMAIN "gbp-glsl-diagnostic-provider"

#include "config.h"

#include "gbp-glsl-diagnostic-provider.h"
#include "gbp-glsl-service.h"

struct _GbpGlslDiagnosticProvider
{
  IdeLspDiagnosticProvider parent_instance;
};

static void
gbp_glsl_diagnostic_provider_load (IdeDiagnosticProvider *provider)
{
  g_autoptr(IdeLspServiceClass) klass = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GLSL_DIAGNOSTIC_PROVIDER (provider));

  klass = g_type_class_ref (GBP_TYPE_GLSL_SERVICE);
  ide_lsp_service_class_bind_client (klass, IDE_OBJECT (provider));

  IDE_EXIT;
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->load = gbp_glsl_diagnostic_provider_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGlslDiagnosticProvider, gbp_glsl_diagnostic_provider, IDE_TYPE_LSP_DIAGNOSTIC_PROVIDER,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER, diagnostic_provider_iface_init))

static void
gbp_glsl_diagnostic_provider_class_init (GbpGlslDiagnosticProviderClass *klass)
{
}

static void
gbp_glsl_diagnostic_provider_init (GbpGlslDiagnosticProvider *self)
{
}
