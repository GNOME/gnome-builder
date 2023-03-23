/* ide-lsp-plugin-search-provider.c
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

#define G_LOG_DOMAIN "ide-lsp-plugin-search-provider"

#include "config.h"

#include <libpeas.h>

#include <libide-code.h>

#include "ide-lsp-search-provider.h"
#include "ide-lsp-plugin-private.h"
#include "ide-lsp-service.h"

typedef struct _IdeLspPluginSearchProviderClass
{
  IdeLspSearchProviderClass  parent_class;
  IdeLspPluginInfo          *info;
} IdeLspPluginSearchProviderClass;

static void
ide_lsp_plugin_search_provider_parent_set (IdeObject *object,
                                           IdeObject *parent)
{
  IdeLspPluginSearchProviderClass *klass;
  g_autoptr(IdeLspServiceClass) service_class = NULL;

  g_assert (IDE_IS_LSP_SEARCH_PROVIDER (object));

  if (parent == NULL)
    return;

  klass = (IdeLspPluginSearchProviderClass *)G_OBJECT_GET_CLASS (object);
  service_class = g_type_class_ref (klass->info->service_type);

  ide_lsp_service_class_bind_client_lazy (service_class, IDE_OBJECT (object));
}

static void
ide_lsp_plugin_search_provider_class_init (IdeLspPluginSearchProviderClass *klass,
                                           IdeLspPluginInfo                *info)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->parent_set = ide_lsp_plugin_search_provider_parent_set;

  klass->info = info;
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
GObject *
ide_lsp_plugin_create_search_provider (guint             n_parameters,
                                       GParameter       *parameters,
                                       IdeLspPluginInfo *info)
{
  ide_lsp_plugin_remove_plugin_info_param (&n_parameters, parameters);

  if G_UNLIKELY (info->search_provider_type == G_TYPE_INVALID)
    {
      g_autofree char *name = g_strconcat (info->module_name, "+SearchProvider", NULL);

      info->search_provider_type =
        g_type_register_static (IDE_TYPE_LSP_SEARCH_PROVIDER,
                                name,
                                &(GTypeInfo) {
                                  sizeof (IdeLspPluginSearchProviderClass),
                                  NULL,
                                  NULL,
                                  (GClassInitFunc)ide_lsp_plugin_search_provider_class_init,
                                  NULL,
                                  ide_lsp_plugin_info_ref (info),
                                  sizeof (IdeLspSearchProvider),
                                  0,
                                  NULL,
                                  NULL,
                                },
                                G_TYPE_FLAG_FINAL);
    }

  return g_object_newv (info->search_provider_type, n_parameters, parameters);
}
G_GNUC_END_IGNORE_DEPRECATIONS
