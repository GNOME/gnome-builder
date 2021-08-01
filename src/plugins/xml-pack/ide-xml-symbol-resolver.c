/* ide-xml-symbol-resolver.c
 *
 * Copyright 2017 Sébastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "xml-symbol-resolver"

#include "ide-xml-service.h"
#include "ide-xml-symbol-tree.h"

#include "ide-xml-symbol-resolver.h"

struct _IdeXmlSymbolResolver
{
  IdeObject parent_instance;
};

static void symbol_resolver_iface_init (IdeSymbolResolverInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeXmlSymbolResolver, ide_xml_symbol_resolver, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER, symbol_resolver_iface_init))

static void
ide_xml_symbol_resolver_lookup_symbol_async (IdeSymbolResolver   *resolver,
                                             IdeLocation   *location,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  IdeXmlSymbolResolver *self = (IdeXmlSymbolResolver *)resolver;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_XML_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_xml_symbol_resolver_lookup_symbol_async);

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_UNKNOWN,
                             "No symbol lookup for xml files.");

  IDE_EXIT;
}

static IdeSymbol *
ide_xml_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver  *resolver,
                                              GAsyncResult       *result,
                                              GError            **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_RESOLVER (resolver), NULL);
  g_return_val_if_fail (IDE_IS_TASK (task), NULL);

  return ide_task_propagate_pointer (task, error);
}

static void
ide_xml_symbol_resolver_get_symbol_tree_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeXmlService *service = (IdeXmlService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeXmlSymbolNode) root_node = NULL;
  IdeXmlSymbolTree *symbol_tree;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_XML_SERVICE (service));

  root_node = ide_xml_service_get_root_node_finish (service, result, &error);
  if (root_node != NULL)
    {
      symbol_tree = ide_xml_symbol_tree_new (root_node);
      ide_task_return_pointer (task, symbol_tree, g_object_unref);
    }
  else
    ide_task_return_error (task, g_steal_pointer (&error));

  IDE_EXIT;
}

static void
ide_xml_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *resolver,
                                               GFile               *file,
                                               GBytes              *contents,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  IdeXmlSymbolResolver *self = (IdeXmlSymbolResolver *)resolver;
  g_autoptr(IdeTask) task = NULL;
  IdeContext *context;
  IdeXmlService *service;

  IDE_ENTRY;

  g_assert (IDE_IS_XML_SYMBOL_RESOLVER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_xml_service_from_context (context);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);
  ide_task_set_source_tag (task, ide_xml_symbol_resolver_get_symbol_tree_async);

  ide_xml_service_get_root_node_async (service,
                                       file,
                                       contents,
                                       cancellable,
                                       ide_xml_symbol_resolver_get_symbol_tree_cb,
                                       g_object_ref (task));

  IDE_EXIT;
}

static IdeSymbolTree *
ide_xml_symbol_resolver_get_symbol_tree_finish (IdeSymbolResolver  *resolver,
                                                GAsyncResult       *result,
                                                GError            **error)
{
  IdeSymbolTree *ret;
  IdeTask *task = (IdeTask *)result;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_RESOLVER (resolver), NULL);
  g_return_val_if_fail (IDE_IS_TASK (task), NULL);

  ret = ide_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

static void
ide_xml_symbol_resolver_class_init (IdeXmlSymbolResolverClass *klass)
{
}

static void
ide_xml_symbol_resolver_init (IdeXmlSymbolResolver *self)
{
}

static void
symbol_resolver_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->lookup_symbol_async = ide_xml_symbol_resolver_lookup_symbol_async;
  iface->lookup_symbol_finish = ide_xml_symbol_resolver_lookup_symbol_finish;
  iface->get_symbol_tree_async = ide_xml_symbol_resolver_get_symbol_tree_async;
  iface->get_symbol_tree_finish = ide_xml_symbol_resolver_get_symbol_tree_finish;
}
