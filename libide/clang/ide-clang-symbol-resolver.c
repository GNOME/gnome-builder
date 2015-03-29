/* ide-clang-symbol-resolver.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "clang-symbol-resolver"

#include "ide-context.h"
#include "ide-clang-service.h"
#include "ide-clang-symbol-resolver.h"
#include "ide-debug.h"
#include "ide-file.h"
#include "ide-source-location.h"
#include "ide-symbol.h"

struct _IdeClangSymbolResolver
{
  IdeSymbolResolver parent_instance;
};

G_DEFINE_TYPE (IdeClangSymbolResolver, ide_clang_symbol_resolver, IDE_TYPE_SYMBOL_RESOLVER)

static void
ide_clang_symbol_resolver_lookup_symbol_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeClangService *service = (IdeClangService *)object;
  g_autoptr(IdeClangTranslationUnit) unit = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  IdeSourceLocation *location;
  GError *error = NULL;

  g_assert (IDE_IS_CLANG_SERVICE (service));
  g_assert (G_IS_TASK (task));

  location = g_task_get_task_data (task);

  unit = ide_clang_service_get_translation_unit_finish (service, result, &error);

  if (unit == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  symbol = ide_clang_translation_unit_lookup_symbol (unit, location, &error);

  if (symbol == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, ide_symbol_ref (symbol), (GDestroyNotify)ide_symbol_unref);
}

static void
ide_clang_symbol_resolver_lookup_symbol_async (IdeSymbolResolver   *resolver,
                                               IdeSourceLocation   *location,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  IdeClangSymbolResolver *self = (IdeClangSymbolResolver *)resolver;
  IdeClangService *service = NULL;
  IdeContext *context;
  IdeFile *file;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_context_get_service_typed (context, IDE_TYPE_CLANG_SERVICE);
  file = ide_source_location_get_file (location);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, ide_source_location_ref (location),
                        (GDestroyNotify)ide_source_location_unref);

  ide_clang_service_get_translation_unit_async (service,
                                                file,
                                                0,
                                                cancellable,
                                                ide_clang_symbol_resolver_lookup_symbol_cb,
                                                g_object_ref (task));

  IDE_EXIT;
}

static IdeSymbol *
ide_clang_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver  *resolver,
                                                GAsyncResult       *result,
                                                GError            **error)
{
  IdeSymbol *ret;
  GTask *task = (GTask *)result;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_RESOLVER (resolver), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

static void
ide_clang_symbol_resolver_get_symbols_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeClangService *service = (IdeClangService *)object;
  g_autoptr(IdeClangTranslationUnit) unit = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GPtrArray) ret = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_SERVICE (service));
  g_assert (G_IS_TASK (task));

  unit = ide_clang_service_get_translation_unit_finish (service, result, &error);

  if (unit == NULL)
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  ret = g_ptr_array_new ();

  g_task_return_pointer (task, g_ptr_array_ref (ret), (GDestroyNotify)g_ptr_array_unref);

  IDE_EXIT;
}

static void
ide_clang_symbol_resolver_get_symbols_async (IdeSymbolResolver   *resolver,
                                             IdeFile             *file,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  IdeClangSymbolResolver *self = (IdeClangSymbolResolver *)resolver;
  IdeClangService *service = NULL;
  IdeContext *context;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_SYMBOL_RESOLVER (self));
  g_assert (IDE_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_context_get_service_typed (context, IDE_TYPE_CLANG_SERVICE);

  task = g_task_new (self, cancellable, callback, user_data);

  ide_clang_service_get_translation_unit_async (service,
                                                file,
                                                0,
                                                cancellable,
                                                ide_clang_symbol_resolver_get_symbols_cb,
                                                g_object_ref (task));

  IDE_EXIT;
}

static GPtrArray *
ide_clang_symbol_resolver_get_symbols_finish (IdeSymbolResolver  *resolver,
                                              GAsyncResult       *result,
                                              GError            **error)
{
  GPtrArray *ret;
  GTask *task = (GTask *)result;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_RESOLVER (resolver), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

static void
ide_clang_symbol_resolver_class_init (IdeClangSymbolResolverClass *klass)
{
  IdeSymbolResolverClass *symbol_resolver_class = IDE_SYMBOL_RESOLVER_CLASS (klass);

  symbol_resolver_class->lookup_symbol_async = ide_clang_symbol_resolver_lookup_symbol_async;
  symbol_resolver_class->lookup_symbol_finish = ide_clang_symbol_resolver_lookup_symbol_finish;
  symbol_resolver_class->get_symbols_async = ide_clang_symbol_resolver_get_symbols_async;
  symbol_resolver_class->get_symbols_finish = ide_clang_symbol_resolver_get_symbols_finish;
}

static void
ide_clang_symbol_resolver_init (IdeClangSymbolResolver *self)
{
}
