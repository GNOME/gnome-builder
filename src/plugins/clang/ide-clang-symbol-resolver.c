/* ide-clang-symbol-resolver.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "clang-symbol-resolver"

#include "config.h"

#include <libide-foundry.h>

#include "ide-clang-client.h"
#include "ide-clang-symbol-resolver.h"

struct _IdeClangSymbolResolver
{
  IdeObject parent_instance;
};

static void symbol_resolver_iface_init (IdeSymbolResolverInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeClangSymbolResolver, ide_clang_symbol_resolver, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER, symbol_resolver_iface_init))

static void
ide_clang_symbol_resolver_lookup_symbol_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeClangClient *client = (IdeClangClient *)object;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(symbol = ide_clang_client_locate_symbol_finish (client, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&symbol),
                             g_object_unref);

  IDE_EXIT;
}

static void
lookup_symbol_flags_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeClangClient) client = NULL;
  g_auto(GStrv) flags = NULL;
  IdeLocation *location;
  GCancellable *cancellable;
  IdeContext *context;
  GFile *file;
  guint line;
  guint column;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  flags = ide_build_system_get_build_flags_finish (build_system, result, NULL);
  context = ide_object_get_context (IDE_OBJECT (build_system));
  client = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CLANG_CLIENT);
  cancellable = ide_task_get_cancellable (task);
  location = ide_task_get_task_data (task);
  file = ide_location_get_file (location);
  line = ide_location_get_line (location);
  column = ide_location_get_line_offset (location);

  ide_clang_client_locate_symbol_async (client,
                                        file,
                                        (const gchar * const *)flags,
                                        line + 1,
                                        column + 1,
                                        cancellable,
                                        ide_clang_symbol_resolver_lookup_symbol_cb,
                                        g_steal_pointer (&task));

  IDE_EXIT;
}

static void
ide_clang_symbol_resolver_lookup_symbol_async (IdeSymbolResolver   *resolver,
                                               IdeLocation   *location,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  IdeClangSymbolResolver *self = (IdeClangSymbolResolver *)resolver;
  g_autoptr(IdeTask) task = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;
  GFile *file;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CLANG_SYMBOL_RESOLVER (self));
  g_return_if_fail (location != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, ide_clang_symbol_resolver_lookup_symbol_async);
  ide_task_set_task_data (task,
                          g_object_ref (location),
                          g_object_unref);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);
  file = ide_location_get_file (location);

  ide_build_system_get_build_flags_async (build_system,
                                          file,
                                          cancellable,
                                          lookup_symbol_flags_cb,
                                          g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeSymbol *
ide_clang_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver  *resolver,
                                                GAsyncResult       *result,
                                                GError            **error)
{
  IdeSymbol *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_SYMBOL_RESOLVER (resolver));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_clang_symbol_resolver_get_symbol_tree_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeClangClient *client = (IdeClangClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeSymbolTree) tree = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(tree = ide_clang_client_get_symbol_tree_finish (client, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_object (task, g_steal_pointer (&tree));

  IDE_EXIT;
}

static void
get_symbol_tree_flags_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeClangClient) client = NULL;
  g_auto(GStrv) flags = NULL;
  GCancellable *cancellable;
  IdeContext *context;
  GFile *file;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  flags = ide_build_system_get_build_flags_finish (build_system, result, NULL);
  context = ide_object_get_context (IDE_OBJECT (build_system));
  client = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CLANG_CLIENT);
  cancellable = ide_task_get_cancellable (task);
  file = ide_task_get_task_data (task);

  ide_clang_client_get_symbol_tree_async (client,
                                          file,
                                          (const gchar * const *)flags,
                                          cancellable,
                                          ide_clang_symbol_resolver_get_symbol_tree_cb,
                                          g_steal_pointer (&task));
}

static void
ide_clang_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *resolver,
                                                 GFile               *file,
                                                 GBytes              *content,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data)
{
  IdeClangSymbolResolver *self = (IdeClangSymbolResolver *)resolver;
  g_autoptr(IdeTask) task = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CLANG_SYMBOL_RESOLVER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, ide_clang_symbol_resolver_get_symbol_tree_async);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  ide_build_system_get_build_flags_async (build_system,
                                          file,
                                          cancellable,
                                          get_symbol_tree_flags_cb,
                                          g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeSymbolTree *
ide_clang_symbol_resolver_get_symbol_tree_finish (IdeSymbolResolver  *resolver,
                                                  GAsyncResult       *result,
                                                  GError            **error)
{
  IdeSymbolTree *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_RESOLVER (resolver), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_clang_symbol_resolver_find_scope_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeClangClient *client = (IdeClangClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(symbol = ide_clang_client_find_nearest_scope_finish (client, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&symbol),
                             g_object_unref);
}

static void
find_nearest_scope_flags_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeClangClient) client = NULL;
  g_auto(GStrv) flags = NULL;
  IdeLocation *location;
  GCancellable *cancellable;
  IdeContext *context;
  GFile *file;
  guint line;
  guint column;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  flags = ide_build_system_get_build_flags_finish (build_system, result, NULL);
  context = ide_object_get_context (IDE_OBJECT (build_system));
  client = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CLANG_CLIENT);
  cancellable = ide_task_get_cancellable (task);
  location = ide_task_get_task_data (task);
  file = ide_location_get_file (location);
  line = ide_location_get_line (location);
  column = ide_location_get_line_offset (location);

  ide_clang_client_find_nearest_scope_async (client,
                                             file,
                                             (const gchar * const *)flags,
                                             line + 1,
                                             column + 1,
                                             cancellable,
                                             ide_clang_symbol_resolver_find_scope_cb,
                                             g_steal_pointer (&task));
}

static void
ide_clang_symbol_resolver_find_nearest_scope_async (IdeSymbolResolver   *symbol_resolver,
                                                    IdeLocation   *location,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
  IdeClangSymbolResolver *self = (IdeClangSymbolResolver *)symbol_resolver;
  g_autoptr(IdeTask) task = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;
  GFile *file;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CLANG_SYMBOL_RESOLVER (self));
  g_return_if_fail (location != NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, ide_clang_symbol_resolver_find_nearest_scope_async);
  ide_task_set_task_data (task,
                          g_object_ref (location),
                          g_object_unref);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);
  file = ide_location_get_file (location);

  ide_build_system_get_build_flags_async (build_system,
                                          file,
                                          cancellable,
                                          find_nearest_scope_flags_cb,
                                          g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeSymbol *
ide_clang_symbol_resolver_find_nearest_scope_finish (IdeSymbolResolver  *resolver,
                                                     GAsyncResult       *result,
                                                     GError            **error)
{
  IdeSymbol *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_RESOLVER (resolver), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_clang_symbol_resolver_class_init (IdeClangSymbolResolverClass *klass)
{
}

static void
symbol_resolver_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->lookup_symbol_async = ide_clang_symbol_resolver_lookup_symbol_async;
  iface->lookup_symbol_finish = ide_clang_symbol_resolver_lookup_symbol_finish;
  iface->get_symbol_tree_async = ide_clang_symbol_resolver_get_symbol_tree_async;
  iface->get_symbol_tree_finish = ide_clang_symbol_resolver_get_symbol_tree_finish;
  iface->find_nearest_scope_async = ide_clang_symbol_resolver_find_nearest_scope_async;
  iface->find_nearest_scope_finish = ide_clang_symbol_resolver_find_nearest_scope_finish;
}

static void
ide_clang_symbol_resolver_init (IdeClangSymbolResolver *self)
{
}
