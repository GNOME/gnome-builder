/* gbp-gjs-symbol-resolver.c
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

#define G_LOG_DOMAIN "gbp-gjs-symbol-resolver"

#include "config.h"

#include <errno.h>
#include <json-glib/json-glib.h>
#include <unistd.h>

#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-gjs-symbol-resolver.h"
#include "gbp-gjs-symbol-tree.h"

struct _GbpGjsSymbolResolver
{
  IdeObject parent_instance;
};

static void
gbp_gjs_symbol_resolver_get_symbol_tree_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GbpGjsSymbolNode) node = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;
  JsonObject *obj;
  JsonNode *root;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_print ("GJS: %s\n", stdout_buf);

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, stdout_buf, -1, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!(root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_OBJECT (root) ||
      !(obj = json_node_get_object (root)) ||
      !(node = gbp_gjs_symbol_node_new (obj)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Reflect.parse() returned invalid data");
      IDE_EXIT;
    }

  ide_task_return_object (task, gbp_gjs_symbol_tree_new (node));

  IDE_EXIT;
}

static void
gbp_gjs_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *resolver,
                                               GFile               *file,
                                               GBytes              *contents,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) script = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *name = NULL;
  int fd;

  IDE_ENTRY;

  g_assert (GBP_IS_GJS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (resolver, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_gjs_symbol_resolver_get_symbol_tree_async);

  script = g_resources_lookup_data ("/plugins/gjs-symbols/parse.js", 0, NULL);
  name = g_file_get_basename (file);

  g_assert (script != NULL);
  g_assert (name != NULL);

  if (!(context = ide_object_ref_context (IDE_OBJECT (resolver))) ||
      !(launcher = ide_foundry_get_launcher_for_context (context, "gjs", "/usr/bin/gjs", NULL)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "A working `gjs` could not be found");
      IDE_EXIT;
    }

  if (contents != NULL)
    fd = ide_foundry_bytes_to_memfd (contents, "gjs-symbols-data");
  else
    fd = ide_foundry_file_to_memfd (file, "gjs-symbols-data");

  if (fd < 0)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to open temporary file: %s",
                                 g_strerror (errno));
      IDE_EXIT;
    }

  ide_subprocess_launcher_set_flags (launcher,
                                     (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                      G_SUBPROCESS_FLAGS_STDERR_SILENCE));
  ide_subprocess_launcher_take_fd (launcher, fd, 3);
  ide_subprocess_launcher_push_argv (launcher, "-c");
  ide_subprocess_launcher_push_argv (launcher, (const char *)g_bytes_get_data (script, NULL));
  ide_subprocess_launcher_push_argv (launcher, name);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         cancellable,
                                         gbp_gjs_symbol_resolver_get_symbol_tree_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeSymbolTree *
gbp_gjs_symbol_resolver_get_symbol_tree_finish (IdeSymbolResolver  *resolver,
                                                GAsyncResult       *result,
                                                GError            **error)
{
  IdeSymbolTree *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_GJS_SYMBOL_RESOLVER (resolver));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
symbol_resolver_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->get_symbol_tree_async = gbp_gjs_symbol_resolver_get_symbol_tree_async;
  iface->get_symbol_tree_finish = gbp_gjs_symbol_resolver_get_symbol_tree_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGjsSymbolResolver, gbp_gjs_symbol_resolver, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER, symbol_resolver_iface_init))

static void
gbp_gjs_symbol_resolver_class_init (GbpGjsSymbolResolverClass *klass)
{
}

static void
gbp_gjs_symbol_resolver_init (GbpGjsSymbolResolver *self)
{
}
