/* ide-lsp-plugin-highlighter.c
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

#define G_LOG_DOMAIN "ide-lsp-plugin-highlighter"

#include "config.h"

#include <libpeas.h>

#include <libide-code.h>

#include "ide-lsp-highlighter.h"
#include "ide-lsp-plugin-private.h"
#include "ide-lsp-service.h"

typedef struct _IdeLspPluginHighlighterClass
{
  IdeLspHighlighterClass  parent_class;
  IdeLspPluginInfo       *info;
} IdeLspPluginHighlighterClass;

static void
ide_lsp_plugin_highlighter_load (IdeHighlighter *resolver)
{
  IdeLspPluginHighlighterClass *klass = (IdeLspPluginHighlighterClass *)(((GTypeInstance *)resolver)->g_class);
  g_autoptr(IdeLspServiceClass) service_class = g_type_class_ref (klass->info->service_type);

  ide_lsp_service_class_bind_client (service_class, IDE_OBJECT (resolver));
}

static void
ide_lsp_plugin_highlighter_class_init (IdeLspPluginHighlighterClass *klass,
                                       IdeLspPluginInfo             *info)
{
  klass->info = info;
}

static void
ide_lsp_plugin_highlighter_iface_init (IdeHighlighterInterface *iface)
{
  iface->load = ide_lsp_plugin_highlighter_load;
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
GObject *
ide_lsp_plugin_create_highlighter (guint             n_parameters,
                                   GParameter       *parameters,
                                   IdeLspPluginInfo *info)
{
  ide_lsp_plugin_remove_plugin_info_param (&n_parameters, parameters);

  if G_UNLIKELY (info->highlighter_type == G_TYPE_INVALID)
    {
      g_autofree char *name = g_strconcat (info->module_name, "+Highlighter", NULL);

      info->highlighter_type =
        g_type_register_static (IDE_TYPE_LSP_HIGHLIGHTER,
                                name,
                                &(GTypeInfo) {
                                  sizeof (IdeLspPluginHighlighterClass),
                                  NULL,
                                  NULL,
                                  (GClassInitFunc)ide_lsp_plugin_highlighter_class_init,
                                  NULL,
                                  ide_lsp_plugin_info_ref (info),
                                  sizeof (IdeLspHighlighter),
                                  0,
                                  NULL,
                                  NULL,
                                },
                                G_TYPE_FLAG_FINAL);
      g_type_add_interface_static (info->highlighter_type,
                                   IDE_TYPE_HIGHLIGHTER,
                                   &(GInterfaceInfo) {
                                     (GInterfaceInitFunc)(void (*) (void))ide_lsp_plugin_highlighter_iface_init,
                                     NULL,
                                     NULL,
                                   });
    }

  return g_object_newv (info->highlighter_type, n_parameters, parameters);
}
G_GNUC_END_IGNORE_DEPRECATIONS
