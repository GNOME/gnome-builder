/* ide-documentation-provider.c
 *
 * Copyright (C) 2017 Lucie Charvat <luci.charvat@gmail.com>
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
 */

#define G_LOG_DOMAIN "ide-documentation-provider"

#include "documentation/ide-documentation-provider.h"

G_DEFINE_INTERFACE (IdeDocumentationProvider, ide_documentation_provider, IDE_TYPE_OBJECT)

static void
ide_documentation_provider_default_init (IdeDocumentationProviderInterface *iface)
{
}

void
ide_documentation_provider_get_info (IdeDocumentationProvider *provider,
                                     IdeDocumentationInfo     *info)
{
  g_return_if_fail (IDE_IS_DOCUMENTATION_PROVIDER (provider));

  return IDE_DOCUMENTATION_PROVIDER_GET_IFACE (provider)->get_info (provider, info);
}

gchar *
ide_documentation_provider_get_name (IdeDocumentationProvider *provider)
{
  g_return_val_if_fail (IDE_IS_DOCUMENTATION_PROVIDER (provider), NULL);

  return IDE_DOCUMENTATION_PROVIDER_GET_IFACE (provider)->get_name (provider);
}

IdeDocumentationContext
ide_documentation_provider_get_context (IdeDocumentationProvider *provider)
{
  g_return_val_if_fail (IDE_IS_DOCUMENTATION_PROVIDER (provider), IDE_DOCUMENTATION_CONTEXT_NONE);

  return IDE_DOCUMENTATION_PROVIDER_GET_IFACE (provider)->get_context (provider);
}
