/* gbp-npm-run-command-provider.c
 *
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-npm-run-command-provider"

#include "config.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-npm-build-system.h"
#include "gbp-npm-run-command-provider.h"

struct _GbpNpmRunCommandProvider
{
  IdeObject parent_instance;
};

static const char * const npm_special_scripts[] = {
  "prepare", "publish", "prepublishOnly", "install", "uninstall", "version", "shrinkwrap",
  NULL
};
static const char * const npm_standard_scripts[] = {
  "test", "start", "stop", "restart",
  NULL
};
static const char *npm_start = "start";

static gboolean
is_ignored_script (const char         *script,
                   const char * const *all_scripts)
{
  if (g_strv_contains (npm_special_scripts, script))
    return TRUE;

  if (g_str_has_prefix (script, "pre"))
    {
      const char *without_pre = script + strlen ("pre");

      if (g_strv_contains (npm_special_scripts, without_pre) ||
          g_strv_contains (npm_standard_scripts, without_pre) ||
          g_strv_contains (all_scripts, without_pre))
        return TRUE;
    }

  if (g_str_has_prefix (script, "post"))
    {
      const char *without_post = script + strlen ("post");

      if (g_strv_contains (npm_special_scripts, without_post) ||
          g_strv_contains (npm_standard_scripts, without_post) ||
          g_strv_contains (all_scripts, without_post))
        return TRUE;
    }

  return FALSE;
}

static void
gbp_npm_run_command_provider_list_commands_worker (IdeTask      *task,
                                                   gpointer      source_object,
                                                   gpointer      task_data,
                                                   GCancellable *cancellable)
{
  const char *package_json = task_data;
  g_autoptr(GFile) package_json_file = NULL;
  g_autoptr(GFile) project_dir = NULL;
  g_autoptr(GFile) server_js = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GArray) all_scripts = NULL;
  g_autoptr(GError) error = NULL;
  JsonObject *root_obj;
  JsonArray *scripts_ar;
  JsonNode *root;
  JsonNode *scripts;
  guint n_items;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_NPM_RUN_COMMAND_PROVIDER (source_object));
  g_assert (package_json != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  package_json_file = g_file_new_for_path (package_json);
  project_dir = g_file_get_parent (package_json_file);
  server_js = g_file_get_child (project_dir, "server.js");

  parser = json_parser_new ();

  if (!json_parser_load_from_mapped_file (parser, package_json, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!(root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_OBJECT (root) ||
      !(root_obj = json_node_get_object (root)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  store = g_list_store_new (IDE_TYPE_RUN_COMMAND);

  /* If there are no scripts, just short-circuit */
  if (!json_object_has_member (root_obj, "scripts") ||
      !(scripts = json_object_get_member (root_obj, "scripts")) ||
      !JSON_NODE_HOLDS_ARRAY (scripts) ||
      !(scripts_ar = json_node_get_array (scripts)))
    IDE_GOTO (complete);

  n_items = json_array_get_length (scripts_ar);
  all_scripts = g_array_sized_new (TRUE, FALSE, sizeof (char *), n_items);

  for (guint i = 0; i < n_items; i++)
    {
      JsonNode *node = json_array_get_element (scripts_ar, i);
      const char *str;
      if (JSON_NODE_HOLDS_VALUE (node) && (str = json_node_get_string (node)))
        g_array_append_val (all_scripts, str);
    }

  /* if no start script is specified, but server.js exists,
   * we can still run "npm start".
   */
  if (!g_strv_contains ((const char * const *)(gpointer)all_scripts->data, "start") &&
      g_file_query_exists (server_js, NULL))
    g_array_append_val (all_scripts, npm_start);

  ide_strv_sort ((char **)(gpointer)all_scripts->data, all_scripts->len);

  for (guint i = 0; i < all_scripts->len; i++)
    {
      const char *script = g_array_index (all_scripts, const char *, i);
      g_autoptr(IdeRunCommand) run_command = NULL;
      g_autofree char *id = NULL;
      g_autofree char *display_name = NULL;
      int priority;

      if (is_ignored_script (script, (const char * const *)(gpointer)all_scripts->data))
        continue;

      id = g_strconcat ("npm:", script, NULL);
      display_name = g_strconcat ("npm run ", script, NULL);

      if (g_strv_contains (IDE_STRV_INIT ("start"), script))
        priority = -10;
      else if (g_strv_contains (IDE_STRV_INIT ("stop", "restart"), script))
        priority = 5;
      else if (g_strv_contains (IDE_STRV_INIT ("test"), script))
        priority = 10;
      else
        priority = 0;

      run_command = ide_run_command_new ();
      ide_run_command_set_id (run_command, id);
      ide_run_command_set_priority (run_command, priority);
      ide_run_command_set_display_name (run_command, display_name);
      ide_run_command_set_cwd (run_command, g_file_peek_path (project_dir));
      ide_run_command_set_argv (run_command, IDE_STRV_INIT ("npm", "run", "--silent", script));

      g_list_store_append (store, run_command);
    }

complete:
  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

  IDE_EXIT;
}

static void
gbp_npm_run_command_provider_list_commands_async (IdeRunCommandProvider *provider,
                                                  GCancellable          *cancellable,
                                                  GAsyncReadyCallback    callback,
                                                  gpointer               user_data)
{
  GbpNpmRunCommandProvider *self = (GbpNpmRunCommandProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *project_dir = NULL;
  g_autofree char *package_json = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_NPM_RUN_COMMAND_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_npm_run_command_provider_list_commands_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_NPM_BUILD_SYSTEM (build_system))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not a npm build system");
      IDE_EXIT;
    }

  project_dir = gbp_npm_build_system_get_project_dir (GBP_NPM_BUILD_SYSTEM (build_system));
  package_json = g_build_filename (project_dir, "package.json", NULL);

  ide_task_set_task_data (task, g_steal_pointer (&package_json), g_free);
  ide_task_run_in_thread (task, gbp_npm_run_command_provider_list_commands_worker);

  IDE_EXIT;
}

static GListModel *
gbp_npm_run_command_provider_list_commands_finish (IdeRunCommandProvider  *provider,
                                                   GAsyncResult           *result,
                                                   GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_NPM_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
run_command_provider_iface_init (IdeRunCommandProviderInterface *iface)
{
  iface->list_commands_async = gbp_npm_run_command_provider_list_commands_async;
  iface->list_commands_finish = gbp_npm_run_command_provider_list_commands_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpNpmRunCommandProvider, gbp_npm_run_command_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_RUN_COMMAND_PROVIDER, run_command_provider_iface_init))

static void
gbp_npm_run_command_provider_class_init (GbpNpmRunCommandProviderClass *klass)
{
}

static void
gbp_npm_run_command_provider_init (GbpNpmRunCommandProvider *self)
{
}
