/* ide-buildconfig-config-provider.c
 *
 * Copyright 2016 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "ide-buildconfig-config-provider"

#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include <libide-core.h>
#include <libide-foundry.h>
#include <libide-threading.h>

#include "ide-buildconfig-config.h"
#include "ide-buildconfig-config-provider.h"

#define DOT_BUILDCONFIG ".buildconfig"

struct _IdeBuildconfigConfigProvider
{
  IdeObject  parent_instance;

  GFileMonitor *file_monitor;
  gulong file_change_sig_id;

  /*
   * A GPtrArray of IdeBuildconfigConfiguration that have been registered.
   * We append/remove to/from this array in our default signal handler for
   * the ::added and ::removed signals.
   */
  GPtrArray *configs;

  /*
   * The GKeyFile that was parsed from disk. We keep this around so that
   * we can persist the changes back without destroying comments.
   */
  GKeyFile *key_file;

  /*
   * Last known modification time of the .buildconfig file loaded.
   */
  GDateTime *mtime;

  /*
   * If we removed items from the keyfile, we need to know that so that
   * we persist it back to disk. We only persist back to disk if this bit
   * is set or if any of our registered configs are "dirty".
   *
   * We try hard to avoid writing .buildconfig files unless we know the
   * user did something to change a config. Otherwise we would liter
   * everyone's projects with .buildconfig files.
   */
  guint key_file_dirty : 1;
};

static gchar *
gen_next_id (const gchar *id)
{
  g_auto(GStrv) parts = g_strsplit (id, "-", 0);
  guint len = g_strv_length (parts);
  const gchar *end;
  guint64 n64;

  if (len == 0)
    goto add_suffix;

  end = parts[len - 1];

  n64 = g_ascii_strtoull (end, (gchar **)&end, 10);
  if (n64 == 0 || n64 == G_MAXUINT64 || *end != 0)
    goto add_suffix;

  g_free (g_steal_pointer (&parts[len -1]));
  parts[len -1] = g_strdup_printf ("%"G_GUINT64_FORMAT, n64+1);
  return g_strjoinv ("-", parts);

add_suffix:
  return g_strdup_printf ("%s-2", id);
}

static gchar *
get_next_id (IdeConfigManager *manager,
             const gchar      *id)
{
  g_autoptr(GPtrArray) tries = NULL;

  g_assert (IDE_IS_CONFIG_MANAGER (manager));

  tries = g_ptr_array_new_with_free_func (g_free);

  while (ide_config_manager_get_config (manager, id))
    {
      g_autofree gchar *next = gen_next_id (id);
      id = next;
      g_ptr_array_add (tries, g_steal_pointer (&next));
    }

  return g_strdup (id);
}

static void
load_string (IdeConfig   *config,
             GKeyFile    *key_file,
             const gchar *group,
             const gchar *key,
             const gchar *property)
{
  g_assert (IDE_IS_CONFIG (config));
  g_assert (key_file != NULL);
  g_assert (group != NULL);
  g_assert (key != NULL);

  if (g_key_file_has_key (key_file, group, key, NULL))
    {
      g_auto(GValue) value = G_VALUE_INIT;

      g_value_init (&value, G_TYPE_STRING);
      g_value_take_string (&value, g_key_file_get_string (key_file, group, key, NULL));
      g_object_set_property (G_OBJECT (config), property, &value);
    }
}

static void
load_strv (IdeConfig   *config,
           GKeyFile    *key_file,
           const gchar *group,
           const gchar *key,
           const gchar *property)
{
  g_assert (IDE_IS_CONFIG (config));
  g_assert (key_file != NULL);
  g_assert (group != NULL);
  g_assert (key != NULL);

  if (g_key_file_has_key (key_file, group, key, NULL))
    {
      g_auto(GStrv) strv = NULL;
      g_auto(GValue) value = G_VALUE_INIT;

      strv = g_key_file_get_string_list (key_file, group, key, NULL, NULL);
      g_value_init (&value, G_TYPE_STRV);
      g_value_take_boxed (&value, g_steal_pointer (&strv));
      g_object_set_property (G_OBJECT (config), property, &value);
    }
}

static void
load_argv (IdeConfig   *config,
           GKeyFile    *key_file,
           const gchar *group,
           const gchar *key,
           const gchar *property)
{
  g_assert (IDE_IS_CONFIG (config));
  g_assert (key_file != NULL);
  g_assert (group != NULL);
  g_assert (key != NULL);

  if (g_key_file_has_key (key_file, group, key, NULL))
    {
      g_autofree char *str = NULL;
      g_auto(GValue) value = G_VALUE_INIT;
      g_auto(GStrv) argv = NULL;
      int argc = 0;

      if ((str = g_key_file_get_string (key_file, group, key, NULL)) &&
          str[0] != 0 &&
          g_shell_parse_argv (str, &argc, &argv, NULL))
        {
          g_value_init (&value, G_TYPE_STRV);
          g_value_take_boxed (&value, g_steal_pointer (&argv));
          g_object_set_property (G_OBJECT (config), property, &value);
        }
    }
}

static void
load_environ (IdeConfig      *config,
              IdeEnvironment *environment,
              GKeyFile       *key_file,
              const gchar    *group)
{
  g_auto(GStrv) keys = NULL;
  gsize len = 0;

  g_assert (IDE_IS_CONFIG (config));
  g_assert (IDE_IS_ENVIRONMENT (environment));
  g_assert (key_file != NULL);
  g_assert (group != NULL);

  keys = g_key_file_get_keys (key_file, group, &len, NULL);

  for (gsize i = 0; i < len; i++)
    {
      g_autofree gchar *value = NULL;

      value = g_key_file_get_string (key_file, group, keys[i], NULL);
      if (value != NULL)
        ide_environment_setenv (environment, keys [i], value);
    }
}

static gboolean
ide_buildconfig_config_build_file (IdeBuildconfigConfigProvider *self,
                                   GFile                       **file,
                                   GDateTime                   **mtime)
{
  g_autofree gchar *path = NULL;
  gboolean file_exists;
  IdeContext *context;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (file != NULL && *file == NULL);
  g_assert (mtime == NULL || *mtime == NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  path = ide_context_build_filename (context, DOT_BUILDCONFIG, NULL);
  file_exists = g_file_test (path, G_FILE_TEST_IS_REGULAR);

  *file = g_file_new_for_path (path);

  if (file_exists && mtime != NULL)
    {
      g_autoptr(GFileInfo) info = NULL;

      info = g_file_query_info (*file,
                                G_FILE_ATTRIBUTE_TIME_MODIFIED","
                                G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC,
                                G_FILE_QUERY_INFO_NONE,
                                NULL,
                                NULL);
      if (info != NULL)
        *mtime = g_file_info_get_modification_date_time (info);
    }

  return file_exists;
}

static IdeConfig *
ide_buildconfig_config_provider_create (IdeBuildconfigConfigProvider *self,
                                        GKeyFile                     *key_file,
                                        const gchar                  *config_id)
{
  g_autoptr(IdeConfig) config = NULL;
  g_autofree gchar *env_group = NULL;
  g_autofree gchar *rt_env_group = NULL;
  IdeEnvironment   *environment;
  IdeEnvironment   *rt_environment;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (key_file != NULL);
  g_assert (config_id != NULL);

  config = g_object_new (IDE_TYPE_BUILDCONFIG_CONFIG,
                         "id", config_id,
                         "parent", self,
                         NULL);

  load_string (config, key_file, config_id, "config-opts", "config-opts");
  load_string (config, key_file, config_id, "name", "display-name");
  load_string (config, key_file, config_id, "run-opts", "run-opts");
  load_string (config, key_file, config_id, "runtime", "runtime-id");
  load_string (config, key_file, config_id, "toolchain", "toolchain-id");
  load_string (config, key_file, config_id, "prefix", "prefix");
  load_string (config, key_file, config_id, "app-id", "app-id");
  load_strv (config, key_file, config_id, "prebuild", "prebuild");
  load_strv (config, key_file, config_id, "postbuild", "postbuild");
  load_argv (config, key_file, config_id, "run-command", "run-command");

  if (g_key_file_has_key (key_file, config_id, "builddir", NULL))
    {
      if (g_key_file_get_boolean (key_file, config_id, "builddir", NULL))
        ide_config_set_locality (config, IDE_BUILD_LOCALITY_OUT_OF_TREE);
      else
        ide_config_set_locality (config, IDE_BUILD_LOCALITY_IN_TREE);
    }

  env_group = g_strdup_printf ("%s.environment", config_id);
  if (g_key_file_has_group (key_file, env_group))
    {
      environment = ide_config_get_environment (config);
      load_environ (config, environment, key_file, env_group);
    }

  rt_env_group = g_strdup_printf ("%s.runtime_environment", config_id);
  if (g_key_file_has_group (key_file, rt_env_group))
    {
      rt_environment = ide_config_get_runtime_environment (config);
      load_environ (config, rt_environment, key_file, rt_env_group);
    }

  return g_steal_pointer (&config);
}

static void
replace_existing_configs_using_keyfile (IdeConfigProvider *provider,
                                        GKeyFile          *new_key_file)
{
  IdeBuildconfigConfigProvider *self = (IdeBuildconfigConfigProvider *)provider;
  IdeConfigManager *manager;
  IdeConfig *current;
  IdeContext *context;
  g_autoptr(GPtrArray) old_configs = NULL;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));

  if (self->configs->len <= 0)
    return;

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_config_manager_from_context (context);
  current = ide_config_manager_get_current (manager);

  old_configs = _g_ptr_array_copy_objects (self->configs);
  for (guint i = 0; i < old_configs->len; i++)
    {
      IdeConfig *old_config;
      const gchar *old_config_id;
      g_autoptr(IdeConfig) new_config = NULL;

      old_config = g_ptr_array_index (old_configs, i);
      old_config_id = ide_config_get_id (old_config);
      if (!g_key_file_has_group (new_key_file, old_config_id))
        {
          ide_config_provider_emit_removed (provider, old_config);
          continue;
        }

      new_config = ide_buildconfig_config_provider_create (self, new_key_file, old_config_id);
      ide_config_set_dirty (new_config, FALSE);
      ide_config_provider_emit_added (provider, new_config);

      if (current == old_config)
        ide_config_manager_set_current (manager, new_config);

      ide_config_provider_emit_removed (provider, old_config);
    }
}

static void
reload_keyfile (IdeConfigProvider *provider,
                GFile             *file,
                GDateTime         *mtime)
{
  IdeBuildconfigConfigProvider *self = (IdeBuildconfigConfigProvider *)provider;
  g_autoptr(IdeConfig) fallback = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *path = NULL;
  g_auto(GStrv) groups = NULL;
  gsize len;
  g_autoptr(GKeyFile) key_file = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (!file || mtime != NULL);

  key_file = g_key_file_new ();

  if (file == NULL)
    IDE_GOTO (add_default);

  path = g_file_get_path (file);
  if (!g_key_file_load_from_file (key_file, path, G_KEY_FILE_KEEP_COMMENTS, &error))
    {
      g_warning ("Failed to load .buildconfig: %s", error->message);
      IDE_GOTO (add_default);
    }

  replace_existing_configs_using_keyfile (provider, key_file);

  groups = g_key_file_get_groups (key_file, &len);
  for (gsize i = 0; i < len; i++)
    {
      g_autoptr(IdeConfig) config = NULL;
      const gchar *group = groups[i];

      if (strchr (group, '.') != NULL)
        continue;

      if (g_key_file_has_group (self->key_file, group))
        continue;

      config = ide_buildconfig_config_provider_create (self, key_file, group);
      ide_config_set_dirty (config, FALSE);
      ide_config_provider_emit_added (provider, config);
    }

  if (self->configs->len > 0)
    IDE_GOTO (complete);

add_default:
  /* "Default" is not translated because .buildconfig can be checked in */
  fallback = g_object_new (IDE_TYPE_BUILDCONFIG_CONFIG,
                           "display-name", "Default",
                           "id", "default",
                           "parent", self,
                           "runtime-id", "host",
                           "toolchain-id", "default",
                           NULL);
  ide_config_set_dirty (fallback, FALSE);
  ide_config_provider_emit_added (provider, fallback);

complete:
  g_clear_pointer (&self->mtime, g_date_time_unref);
  g_clear_pointer (&self->key_file, g_key_file_free);
  self->key_file = g_steal_pointer (&key_file);
  self->key_file_dirty = FALSE;
  self->mtime = mtime ? g_date_time_ref (mtime) : NULL;

  IDE_EXIT;
}

static void
ide_buildconfig_config_provider_file_changed_cb (IdeBuildconfigConfigProvider *self,
                                                 GFile                        *file,
                                                 GFile                        *other_file,
                                                 GFileMonitorEvent             event,
                                                 GFileMonitor                 *file_monitor)
{
  g_autoptr(GFile) cfg_file = NULL;
  g_autoptr(GDateTime) cfg_mtime = NULL;
  gboolean should_reload;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (G_IS_FILE_MONITOR (file_monitor));

  if (event != G_FILE_MONITOR_EVENT_CHANGED && event != G_FILE_MONITOR_EVENT_CREATED)
    return;

  if (!ide_buildconfig_config_build_file (self, &cfg_file, &cfg_mtime))
    return;

  if (event == G_FILE_MONITOR_EVENT_CREATED)
    /* If the file was newly created, load it if we don't have a recorded mtime, so we
     * know we did not load a config previously.
     */
    should_reload = self->mtime == NULL;
  else
    /*
     * If it was updated, only reload file when mtime available. Otherwise it might drop
     * the config edited in the project config editor GUI.
     */
    should_reload = self->mtime != NULL &&
                    cfg_mtime != NULL &&
                    g_date_time_compare (self->mtime, cfg_mtime) < 0;

  if (should_reload)
    reload_keyfile (IDE_CONFIG_PROVIDER (self), cfg_file, cfg_mtime);
}

static void
ide_buildconfig_config_provider_block_monitor (IdeBuildconfigConfigProvider *self)
{
  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));

    g_signal_handler_block (self->file_monitor,
                            self->file_change_sig_id);
}

static void
ide_buildconfig_config_provider_unblock_monitor (IdeBuildconfigConfigProvider *self)
{
  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));

  g_signal_handler_unblock (self->file_monitor,
                            self->file_change_sig_id);
}

static void
ide_buildconfig_config_provider_start_monitor (IdeBuildconfigConfigProvider *self,
                                               GFile                        *file)
{
  gulong sig_id;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (self->file_monitor == NULL);
  g_assert (self->file_change_sig_id == 0);

  self->file_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
  g_file_monitor_set_rate_limit (self->file_monitor, 1000);
  sig_id = g_signal_connect_object (self->file_monitor,
                                    "changed",
                                    G_CALLBACK (ide_buildconfig_config_provider_file_changed_cb),
                                    self,
                                    G_CONNECT_SWAPPED);
  self->file_change_sig_id = sig_id;
}

static void
ide_buildconfig_config_provider_load_async (IdeConfigProvider   *provider,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  IdeBuildconfigConfigProvider *self = (IdeBuildconfigConfigProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GDateTime) mtime = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (self->key_file == NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buildconfig_config_provider_load_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  self->key_file = g_key_file_new ();

  /*
   * We could do this in a thread, but it's not really worth it. We want these
   * configs loaded ASAP, and nothing can really progress until it's loaded
   * anyway.
   */

  if (!ide_buildconfig_config_build_file (self, &file, &mtime))
    reload_keyfile (provider, NULL, NULL);
  else
    reload_keyfile (provider, file, mtime);

  ide_buildconfig_config_provider_start_monitor (self, file);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static gboolean
ide_buildconfig_config_provider_load_finish (IdeConfigProvider  *provider,
                                             GAsyncResult       *result,
                                             GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_buildconfig_config_provider_append_env (IdeEnvironment *env,
                                            gchar          *env_group,
                                            GKeyFile       *key_file)
{
  guint n_items;

  /*
   * Remove all environment keys that are no longer specified in the
   * environment. This allows us to just do a single pass of additions
   * from the environment below.
   */
  if (g_key_file_has_group (key_file, env_group))
    {
      g_auto(GStrv) keys = NULL;

      if (NULL != (keys = g_key_file_get_keys (key_file, env_group, NULL, NULL)))
        {
          for (guint j = 0; keys [j]; j++)
            {
              if (!ide_environment_getenv (env, keys [j]))
                g_key_file_remove_key (key_file, env_group, keys [j], NULL);
            }
        }
    }

  n_items = g_list_model_get_n_items (G_LIST_MODEL (env));

  for (guint j = 0; j < n_items; j++)
    {
      g_autoptr(IdeEnvironmentVariable) var = NULL;
      const gchar *key;
      const gchar *value;

      var = g_list_model_get_item (G_LIST_MODEL (env), j);
      key = ide_environment_variable_get_key (var);
      value = ide_environment_variable_get_value (var);

      if (!ide_str_empty0 (key))
        g_key_file_set_string (key_file, env_group, key, value ?: "");
    }
}

static void
ide_buildconfig_config_provider_save_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeBuildconfigConfigProvider *self;
  GFile *file = (GFile *)object;
  g_autoptr(GFile) cfg_file = NULL;
  g_autoptr(GDateTime) cfg_mtime = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  if (ide_buildconfig_config_build_file (self, &cfg_file, &cfg_mtime))
    {
      g_return_if_fail (g_file_equal (file, cfg_file));

      g_clear_pointer (&self->mtime, g_date_time_unref);
      self->mtime = g_steal_pointer (&cfg_mtime);

      /*
       * Only unblock when the file exist, otherwise wait until a successful save.
       */
      ide_buildconfig_config_provider_unblock_monitor (self);
    }
}

static void
ide_buildconfig_config_provider_save_async (IdeConfigProvider   *provider,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  IdeBuildconfigConfigProvider *self = (IdeBuildconfigConfigProvider *)provider;
  g_autoptr(GHashTable) group_names = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) groups = NULL;
  g_autofree gchar *data = NULL;
  IdeConfigManager *manager;
  IdeContext *context;
  gboolean dirty = FALSE;
  gsize length = 0;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (self->key_file != NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_buildconfig_config_provider_save_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  dirty = self->key_file_dirty;

  /* If no configs are dirty, short circuit to avoid writing any files to disk. */
  for (guint i = 0; !dirty && i < self->configs->len; i++)
    {
      IdeConfig *config = g_ptr_array_index (self->configs, i);
      dirty |= ide_config_get_dirty (config);
    }

  if (!dirty)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_config_manager_from_context (context);

  ide_buildconfig_config_build_file (self, &file, NULL);

  /*
   * We keep the GKeyFile around from when we parsed .buildconfig, so that we
   * can try to preserve comments and such when writing back.
   *
   * This means that we need to fill in all our known configuration sections,
   * and then remove any that were removed since we were parsed it last.
   */

  group_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (guint i = 0; i < self->configs->len; i++)
    {
      IdeConfig *config = g_ptr_array_index (self->configs, i);
      g_autofree gchar *env_group = NULL;
      g_autofree gchar *rt_env_group = NULL;
      const gchar *config_id;
      IdeEnvironment *env;
      IdeEnvironment *rt_env;

      if (!ide_config_get_dirty (config))
        continue;

      config_id = ide_config_get_id (config);
      env_group = g_strdup_printf ("%s.environment", config_id);
      rt_env_group = g_strdup_printf ("%s.runtime_environment", config_id);

      /*
       * Track our known group names, so we can remove missing names after
       * we've updated the GKeyFile.
       */
      g_hash_table_insert (group_names, g_strdup (config_id), NULL);
      g_hash_table_insert (group_names, g_strdup (env_group), NULL);
      g_hash_table_insert (group_names, g_strdup (rt_env_group), NULL);

#define PERSIST_STRING_KEY(key, getter) \
      g_key_file_set_string (self->key_file, config_id, key, \
                             ide_config_##getter (config) ?: "")
#define PERSIST_STRV_KEY(key, getter) G_STMT_START { \
      const gchar * const *val = ide_buildconfig_config_##getter (IDE_BUILDCONFIG_CONFIG (config)); \
      gsize vlen = val ? g_strv_length ((gchar **)val) : 0; \
      g_key_file_set_string_list (self->key_file, config_id, key, val, vlen); \
} G_STMT_END
#define PERSIST_ARGV_KEY(key, getter) G_STMT_START { \
      const gchar * const *val = ide_buildconfig_config_##getter (IDE_BUILDCONFIG_CONFIG (config)); \
      if (val) \
        { \
          g_autofree char *str = g_strjoinv (" ", (char **)val); \
          g_key_file_set_string (self->key_file, config_id, key, str); \
        } \
      else \
        g_key_file_set_string (self->key_file, config_id, key, ""); \
} G_STMT_END

      PERSIST_STRING_KEY ("name", get_display_name);
      PERSIST_STRING_KEY ("runtime", get_runtime_id);
      PERSIST_STRING_KEY ("toolchain", get_toolchain_id);
      PERSIST_STRING_KEY ("config-opts", get_config_opts);
      PERSIST_STRING_KEY ("run-opts", get_run_opts);
      PERSIST_STRING_KEY ("prefix", get_prefix);
      PERSIST_STRING_KEY ("app-id", get_app_id);
      PERSIST_STRV_KEY ("postbuild", get_postbuild);
      PERSIST_STRV_KEY ("prebuild", get_prebuild);
      PERSIST_ARGV_KEY ("run-command", get_run_command);

#undef PERSIST_STRING_KEY
#undef PERSIST_STRV_KEY

      if (ide_config_get_locality (config) == IDE_BUILD_LOCALITY_IN_TREE)
        g_key_file_set_boolean (self->key_file, config_id, "builddir", FALSE);
      else if (ide_config_get_locality (config) == IDE_BUILD_LOCALITY_OUT_OF_TREE)
        g_key_file_set_boolean (self->key_file, config_id, "builddir", TRUE);
      else
        g_key_file_remove_key (self->key_file, config_id, "builddir", NULL);

      if (config == ide_config_manager_get_current (manager))
        g_key_file_set_boolean (self->key_file, config_id, "default", TRUE);
      else
        g_key_file_remove_key (self->key_file, config_id, "default", NULL);

      env = ide_config_get_environment (config);
      ide_buildconfig_config_provider_append_env(env, env_group, self->key_file);

      rt_env = ide_config_get_runtime_environment (config);
      ide_buildconfig_config_provider_append_env(rt_env, rt_env_group, self->key_file);

      ide_config_set_dirty (config, FALSE);
    }

  /* Now truncate any old groups in the keyfile. */
  if (NULL != (groups = g_key_file_get_groups (self->key_file, NULL)))
    {
      for (guint i = 0; groups [i]; i++)
        {
          if (!g_hash_table_contains (group_names, groups [i]))
            g_key_file_remove_group (self->key_file, groups [i], NULL);
        }
    }

  if (!(data = g_key_file_to_data (self->key_file, &length, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self->key_file_dirty = FALSE;

  if (length == 0)
    {
      /* Remove the file if it exists, since it would be empty */
      g_file_delete (file, cancellable, NULL);
      ide_task_return_boolean (task, TRUE);
      return;
    }

  bytes = g_bytes_new_take (g_steal_pointer (&data), length);

  ide_buildconfig_config_provider_block_monitor (self);
  g_file_replace_contents_bytes_async (file,
                                       bytes,
                                       NULL,
                                       FALSE,
                                       G_FILE_CREATE_NONE,
                                       cancellable,
                                       ide_buildconfig_config_provider_save_cb,
                                       g_steal_pointer (&task));
}

static gboolean
ide_buildconfig_config_provider_save_finish (IdeConfigProvider  *provider,
                                             GAsyncResult       *result,
                                             GError            **error)
{
  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_buildconfig_config_provider_delete (IdeConfigProvider *provider,
                                        IdeConfig         *config)
{
  IdeBuildconfigConfigProvider *self = (IdeBuildconfigConfigProvider *)provider;
  g_autoptr(IdeConfig) hold = NULL;
  g_autofree gchar *env = NULL;
  const gchar *config_id;
  gboolean had_group;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (IDE_IS_BUILDCONFIG_CONFIG (config));
  g_assert (self->key_file != NULL);
  g_assert (self->configs->len > 0);

  hold = g_object_ref (config);

  if (!g_ptr_array_remove (self->configs, hold))
    {
      g_critical ("No such configuration %s",
                  ide_config_get_id (hold));
      return;
    }

  config_id = ide_config_get_id (config);
  had_group = g_key_file_has_group (self->key_file, config_id);
  env = g_strdup_printf ("%s.environment", config_id);
  g_key_file_remove_group (self->key_file, config_id, NULL);
  g_key_file_remove_group (self->key_file, env, NULL);

  self->key_file_dirty = had_group;

  /*
   * If we removed our last buildconfig, synthesize a new one to replace it so
   * that we never have no configurations available. We add it before we remove
   * @config so that we never have zero configurations available.
   *
   * At some point in the future we might want a read only NULL configuration
   * for fallback, and group configs by type or something.  But until we have
   * designs for that, this will do.
   */
  if (self->configs->len == 0)
    {
      g_autoptr(IdeConfig) new_config = NULL;

      /* "Default" is not translated because .buildconfig can be checked in */
      new_config = g_object_new (IDE_TYPE_BUILDCONFIG_CONFIG,
                                 "display-name", "Default",
                                 "id", "default",
                                 "parent", self,
                                 "runtime-id", "host",
                                 "toolchain-id", "default",
                                 NULL);

      /*
       * Only persist this back if there was data in the keyfile
       * before we were requested to delete the build-config.
       */
      ide_config_set_dirty (new_config, had_group);
      ide_config_provider_emit_added (provider, new_config);
    }

  ide_config_provider_emit_removed (provider, hold);
}

static void
ide_buildconfig_config_provider_duplicate (IdeConfigProvider *provider,
                                           IdeConfig         *config)
{
  IdeBuildconfigConfigProvider *self = (IdeBuildconfigConfigProvider *)provider;
  g_autoptr(IdeConfig) new_config = NULL;
  g_autofree GParamSpec **pspecs = NULL;
  g_autofree gchar *new_config_id = NULL;
  g_autofree gchar *new_name = NULL;
  IdeConfigManager *manager;
  IdeEnvironment *env;
  const gchar *config_id;
  const gchar *name;
  IdeContext *context;
  guint n_pspecs = 0;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (IDE_IS_CONFIG (config));
  g_assert (IDE_IS_BUILDCONFIG_CONFIG (config));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  manager = ide_config_manager_from_context (context);
  g_assert (IDE_IS_CONFIG_MANAGER (manager));

  config_id = ide_config_get_id (config);
  g_return_if_fail (config_id != NULL);

  new_config_id = get_next_id (manager, config_id);
  g_return_if_fail (new_config_id != NULL);

  name = ide_config_get_display_name (config);
  /* translators: %s is replaced with the name of the configuration */
  new_name = g_strdup_printf (_("%s (Copy)"), name);

  env = ide_config_get_environment (config);

  new_config = g_object_new (IDE_TYPE_BUILDCONFIG_CONFIG,
                             "id", new_config_id,
                             "display-name", new_name,
                             "parent", self,
                             NULL);

  ide_environment_copy_into (env, ide_config_get_environment (new_config), TRUE);

  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (new_config), &n_pspecs);

  for (guint i = 0; i < n_pspecs; i++)
    {
      GParamSpec *pspec = pspecs[i];

      if (g_str_equal (pspec->name, "id") ||
          g_str_equal (pspec->name, "display-name") ||
          g_type_is_a (pspec->value_type, G_TYPE_BOXED) ||
          g_type_is_a (pspec->value_type, G_TYPE_OBJECT))
        continue;


      if ((pspec->flags & G_PARAM_READWRITE) == G_PARAM_READWRITE &&
          (pspec->flags & G_PARAM_CONSTRUCT_ONLY) == 0)
        {
          GValue value = G_VALUE_INIT;

          g_value_init (&value, pspec->value_type);
          g_object_get_property (G_OBJECT (config), pspec->name, &value);
          g_object_set_property (G_OBJECT (new_config), pspec->name, &value);
        }
    }

  ide_config_set_dirty (new_config, TRUE);
  ide_config_provider_emit_added (provider, new_config);
}

static void
ide_buildconfig_config_provider_unload (IdeConfigProvider *provider)
{
  IdeBuildconfigConfigProvider *self = (IdeBuildconfigConfigProvider *)provider;
  g_autoptr(GPtrArray) configs = NULL;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (self->configs != NULL);

  ide_buildconfig_config_provider_block_monitor (self);

  configs = g_steal_pointer (&self->configs);
  self->configs = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < configs->len; i++)
    {
      IdeConfig *config = g_ptr_array_index (configs, i);
      ide_config_provider_emit_removed (provider, config);
    }
}

static void
ide_buildconfig_config_provider_added (IdeConfigProvider *provider,
                                       IdeConfig         *config)
{
  IdeBuildconfigConfigProvider *self = (IdeBuildconfigConfigProvider *)provider;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (IDE_IS_CONFIG (config));
  g_assert (self->configs != NULL);

  g_ptr_array_add (self->configs, g_object_ref (config));
}

static void
ide_buildconfig_config_provider_removed (IdeConfigProvider *provider,
                                         IdeConfig         *config)
{
  IdeBuildconfigConfigProvider *self = (IdeBuildconfigConfigProvider *)provider;

  g_assert (IDE_IS_BUILDCONFIG_CONFIG_PROVIDER (self));
  g_assert (IDE_IS_CONFIG (config));
  g_assert (self->configs != NULL);

  /* It's possible we already removed it by now */
  g_ptr_array_remove (self->configs, config);

  ide_object_destroy (IDE_OBJECT (config));
}

static void
configuration_provider_iface_init (IdeConfigProviderInterface *iface)
{
  iface->added = ide_buildconfig_config_provider_added;
  iface->removed = ide_buildconfig_config_provider_removed;
  iface->load_async = ide_buildconfig_config_provider_load_async;
  iface->load_finish = ide_buildconfig_config_provider_load_finish;
  iface->save_async = ide_buildconfig_config_provider_save_async;
  iface->save_finish = ide_buildconfig_config_provider_save_finish;
  iface->delete = ide_buildconfig_config_provider_delete;
  iface->duplicate = ide_buildconfig_config_provider_duplicate;
  iface->unload = ide_buildconfig_config_provider_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeBuildconfigConfigProvider,
                         ide_buildconfig_config_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CONFIG_PROVIDER,
                                                configuration_provider_iface_init))

static void
ide_buildconfig_config_provider_destroy (IdeObject *object)
{
  IdeBuildconfigConfigProvider *self = (IdeBuildconfigConfigProvider *)object;

  g_clear_signal_handler (&self->file_change_sig_id, self->file_monitor);
  g_clear_object (&self->file_monitor);

  g_clear_pointer (&self->mtime, g_date_time_unref);
  g_clear_pointer (&self->configs, g_ptr_array_unref);
  g_clear_pointer (&self->key_file, g_key_file_free);

  IDE_OBJECT_CLASS (ide_buildconfig_config_provider_parent_class)->destroy (object);
}

static void
ide_buildconfig_config_provider_class_init (IdeBuildconfigConfigProviderClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_buildconfig_config_provider_destroy;
}

static void
ide_buildconfig_config_provider_init (IdeBuildconfigConfigProvider *self)
{
  self->configs = g_ptr_array_new_with_free_func (g_object_unref);
}
