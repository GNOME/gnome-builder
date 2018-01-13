/* ide-buildconfig-configuration-provider.c
 *
 * Copyright © 2016 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "ide-buildconfig-configuration-provider"

#include <dazzle.h>
#include <gio/gio.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buildconfig/ide-buildconfig-configuration.h"
#include "buildconfig/ide-buildconfig-configuration-provider.h"
#include "buildsystem/ide-configuration-manager.h"
#include "buildsystem/ide-configuration-provider.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-environment.h"
#include "vcs/ide-vcs.h"

#define DOT_BUILDCONFIG ".buildconfig"
#define WRITEBACK_TIMEOUT_SECS 2

struct _IdeBuildconfigConfigurationProvider
{
  GObject                  parent_instance;

  IdeConfigurationManager *manager;
  GPtrArray               *configurations;
  GKeyFile                *key_file;

  guint                    writeback_handler;
  guint                    change_count;
};

static void configuration_provider_iface_init (IdeConfigurationProviderInterface *);

G_DEFINE_TYPE_EXTENDED (IdeBuildconfigConfigurationProvider, ide_buildconfig_configuration_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_CONFIGURATION_PROVIDER,
                                               configuration_provider_iface_init))

static void ide_buildconfig_configuration_provider_unload (IdeConfigurationProvider *provider, IdeConfigurationManager *manager);

static void
ide_buildconfig_configuration_provider_save_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GFile *file = (GFile *)object;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

void
ide_buildconfig_configuration_provider_save_async (IdeConfigurationProvider *provider,
                                                   GCancellable             *cancellable,
                                                   GAsyncReadyCallback       callback,
                                                   gpointer                  user_data)
{
  IdeBuildconfigConfigurationProvider *self = (IdeBuildconfigConfigurationProvider *)provider;
  g_autoptr(GHashTable) group_names = NULL;
  g_autoptr(GTask) task = NULL;
  g_auto(GStrv) groups = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  gchar *data;
  gsize length = 0;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_buildconfig_configuration_provider_save_async);

  if (self->configurations == NULL || self->change_count == 0)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  self->change_count = 0;

  context = ide_object_get_context (IDE_OBJECT (self->manager));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  file = g_file_get_child (workdir, DOT_BUILDCONFIG);

  /*
   * NOTE:
   *
   * We keep the GKeyFile around from when we parsed .buildconfig, so that
   * we can try to preserve comments and such when writing back.
   *
   * This means that we need to fill in all our known configuration
   * sections, and then remove any that were removed since we were
   * parsed it last.
   */

  if (self->key_file == NULL)
    self->key_file = g_key_file_new ();

  group_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (guint i = 0; i < self->configurations->len; i++)
    {
      IdeConfiguration *configuration = g_ptr_array_index (self->configurations, i);
      IdeEnvironment *environment;
      guint n_items;
      guint j;
      gchar *group;
      gchar *group_environ;

      group = g_strdup (ide_configuration_get_id (configuration));
      group_environ = g_strdup_printf ("%s.environment", group);

      /*
       * Track our known group names, so we can remove missing names after
       * we've updated the GKeyFile.
       */
      g_hash_table_insert (group_names, group, NULL);
      g_hash_table_insert (group_names, group_environ, NULL);

#define PERSIST_STRING_KEY(key, getter) \
      g_key_file_set_string (self->key_file, group, key, \
                             ide_configuration_##getter (configuration) ?: "")
      PERSIST_STRING_KEY ("name", get_display_name);
      PERSIST_STRING_KEY ("device", get_device_id);
      PERSIST_STRING_KEY ("runtime", get_runtime_id);
      PERSIST_STRING_KEY ("config-opts", get_config_opts);
      PERSIST_STRING_KEY ("run-opts", get_run_opts);
      PERSIST_STRING_KEY ("prefix", get_prefix);
      PERSIST_STRING_KEY ("app-id", get_app_id);
#undef PERSIST_STRING_KEY

      if (configuration == ide_configuration_manager_get_current (self->manager))
        g_key_file_set_boolean (self->key_file, group, "default", TRUE);
      else
        g_key_file_remove_key (self->key_file, group, "default", NULL);

      environment = ide_configuration_get_environment (configuration);

      /*
       * Remove all environment keys that are no longer specified in the
       * environment. This allows us to just do a single pass of additions
       * from the environment below.
       */
      if (g_key_file_has_group (self->key_file, group_environ))
        {
          g_auto(GStrv) keys = NULL;

          if (NULL != (keys = g_key_file_get_keys (self->key_file, group_environ, NULL, NULL)))
            {
              for (j = 0; keys [j]; j++)
                {
                  if (!ide_environment_getenv (environment, keys [j]))
                    g_key_file_remove_key (self->key_file, group_environ, keys [j], NULL);
                }
            }
        }

      n_items = g_list_model_get_n_items (G_LIST_MODEL (environment));

      for (j = 0; j < n_items; j++)
        {
          g_autoptr(IdeEnvironmentVariable) var = NULL;
          const gchar *key;
          const gchar *value;

          var = g_list_model_get_item (G_LIST_MODEL (environment), j);
          key = ide_environment_variable_get_key (var);
          value = ide_environment_variable_get_value (var);

          if (!dzl_str_empty0 (key))
            g_key_file_set_string (self->key_file, group_environ, key, value ?: "");
        }
    }

  /*
   * Now truncate any old groups in the keyfile.
   */
  if (NULL != (groups = g_key_file_get_groups (self->key_file, NULL)))
    {
      for (guint i = 0; groups [i]; i++)
        {
          if (!g_hash_table_contains (group_names, groups [i]))
            g_key_file_remove_group (self->key_file, groups [i], NULL);
        }
    }

  if (NULL == (data = g_key_file_to_data (self->key_file, &length, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  bytes = g_bytes_new_take (data, length);

  g_file_replace_contents_bytes_async (file,
                                       bytes,
                                       NULL,
                                       FALSE,
                                       G_FILE_CREATE_NONE,
                                       cancellable,
                                       ide_buildconfig_configuration_provider_save_cb,
                                       g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_buildconfig_configuration_provider_save_finish (IdeConfigurationProvider  *provider,
                                                    GAsyncResult              *result,
                                                    GError                   **error)
{
  IdeBuildconfigConfigurationProvider *self = (IdeBuildconfigConfigurationProvider *)provider;

  g_return_val_if_fail (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
ide_buildconfig_configuration_provider_do_writeback (gpointer data)
{
  IdeBuildconfigConfigurationProvider *self = data;

  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));

  self->writeback_handler = 0;

  ide_buildconfig_configuration_provider_save_async (IDE_CONFIGURATION_PROVIDER (self), NULL, NULL, NULL);

  return G_SOURCE_REMOVE;
}

static void
ide_buildconfig_configuration_provider_queue_writeback (IdeBuildconfigConfigurationProvider *self)
{
  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));

  IDE_ENTRY;

  if (self->writeback_handler != 0)
    g_source_remove (self->writeback_handler);

  self->writeback_handler = g_timeout_add_seconds (WRITEBACK_TIMEOUT_SECS,
                                                   ide_buildconfig_configuration_provider_do_writeback,
                                                   self);

  IDE_EXIT;
}

static void
ide_buildconfig_configuration_provider_changed (IdeBuildconfigConfigurationProvider *self,
                                                IdeConfiguration *configuration)
{
  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  self->change_count++;

  ide_buildconfig_configuration_provider_queue_writeback (self);
}

static void
load_string (IdeConfiguration *configuration,
             GKeyFile         *key_file,
             const gchar      *group,
             const gchar      *key,
             const gchar      *property)
{
  g_assert (IDE_IS_CONFIGURATION (configuration));
  g_assert (key_file != NULL);
  g_assert (group != NULL);
  g_assert (key != NULL);

  if (g_key_file_has_key (key_file, group, key, NULL))
    {
      g_auto(GValue) value = G_VALUE_INIT;

      g_value_init (&value, G_TYPE_STRING);
      g_value_take_string (&value, g_key_file_get_string (key_file, group, key, NULL));
      g_object_set_property (G_OBJECT (configuration), property, &value);
    }
}

static void
load_environ (IdeConfiguration *configuration,
              GKeyFile         *key_file,
              const gchar      *group)
{
  IdeEnvironment *environment;
  g_auto(GStrv) keys = NULL;

  g_assert (IDE_IS_CONFIGURATION (configuration));
  g_assert (key_file != NULL);
  g_assert (group != NULL);

  environment = ide_configuration_get_environment (configuration);
  keys = g_key_file_get_keys (key_file, group, NULL, NULL);

  if (keys != NULL)
    {
      guint i;

      for (i = 0; keys [i]; i++)
        {
          g_autofree gchar *value = NULL;

          value = g_key_file_get_string (key_file, group, keys [i], NULL);

          if (value != NULL)
            ide_environment_setenv (environment, keys [i], value);
        }
    }
}

static gboolean
ide_buildconfig_configuration_provider_load_group (IdeBuildconfigConfigurationProvider  *self,
                                                   GKeyFile                             *key_file,
                                                   const gchar                          *group,
                                                   GPtrArray                            *configs,
                                                   GError                              **error)
{
  g_autoptr(IdeConfiguration) configuration = NULL;
  g_autofree gchar *env_group = NULL;
  IdeContext *context;

  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));
  g_assert (key_file != NULL);
  g_assert (group != NULL);

  context = ide_object_get_context (IDE_OBJECT (self->manager));

  configuration = g_object_new (IDE_TYPE_BUILDCONFIG_CONFIGURATION,
                                "id", group,
                                "context", context,
                                NULL);

  load_string (configuration, key_file, group, "config-opts", "config-opts");
  load_string (configuration, key_file, group, "device", "device-id");
  load_string (configuration, key_file, group, "name", "display-name");
  load_string (configuration, key_file, group, "run-opts", "run-opts");
  load_string (configuration, key_file, group, "runtime", "runtime-id");
  load_string (configuration, key_file, group, "prefix", "prefix");
  load_string (configuration, key_file, group, "app-id", "app-id");

  if (g_key_file_has_key (key_file, group, "prebuild", NULL))
    {
      g_auto(GStrv) commands = NULL;

      commands = g_key_file_get_string_list (key_file, group, "prebuild", NULL, NULL);
      ide_buildconfig_configuration_set_prebuild (IDE_BUILDCONFIG_CONFIGURATION (configuration),
                                                  (const gchar * const *)commands);
    }

  if (g_key_file_has_key (key_file, group, "postbuild", NULL))
    {
      g_auto(GStrv) commands = NULL;

      commands = g_key_file_get_string_list (key_file, group, "postbuild", NULL, NULL);
      ide_buildconfig_configuration_set_postbuild (IDE_BUILDCONFIG_CONFIGURATION (configuration),
                                                   (const gchar * const *)commands);
    }

  env_group = g_strdup_printf ("%s.environment", group);

  if (g_key_file_has_group (key_file, env_group))
    load_environ (configuration, key_file, env_group);

  ide_configuration_set_dirty (configuration, FALSE);

  if (g_key_file_get_boolean (key_file, group, "default", NULL))
    g_object_set_data (G_OBJECT (configuration), "WAS_DEFAULT", GINT_TO_POINTER (TRUE));

  g_signal_connect_object (configuration,
                           "changed",
                           G_CALLBACK (ide_buildconfig_configuration_provider_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_ptr_array_add (configs, g_steal_pointer (&configuration));

  return TRUE;
}

static gboolean
ide_buildconfig_configuration_provider_restore (IdeBuildconfigConfigurationProvider  *self,
                                                GFile                                *file,
                                                GPtrArray                            *configs,
                                                GCancellable                         *cancellable,
                                                GError                              **error)
{
  g_autofree gchar *contents = NULL;
  g_auto(GStrv) groups = NULL;
  gsize length = 0;
  guint i;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));
  g_assert (self->key_file == NULL);
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->key_file = g_key_file_new ();

  if (!g_file_load_contents (file, cancellable, &contents, &length, NULL, error))
    IDE_RETURN (FALSE);

  if (!g_key_file_load_from_data (self->key_file,
                                  contents,
                                  length,
                                  G_KEY_FILE_KEEP_COMMENTS,
                                  error))
    IDE_RETURN (FALSE);

  groups = g_key_file_get_groups (self->key_file, NULL);

  for (i = 0; groups [i]; i++)
    {
      if (g_str_has_suffix (groups [i], ".environment"))
        continue;

      if (!ide_buildconfig_configuration_provider_load_group (self, self->key_file, groups [i], configs, error))
        IDE_RETURN (FALSE);
    }

  IDE_RETURN (TRUE);
}

static void
ide_buildconfig_configuration_provider_load_worker (GTask        *task,
                                                    gpointer      source_object,
                                                    gpointer      task_data,
                                                    GCancellable *cancellable)
{
  IdeBuildconfigConfigurationProvider *self = source_object;
  g_autoptr(GFile) settings_file = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) configs = NULL;
  IdeConfigurationManager *manager = task_data;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  configs = g_ptr_array_new_with_free_func (g_object_unref);

  context = ide_object_get_context (IDE_OBJECT (manager));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  settings_file = g_file_get_child (workdir, DOT_BUILDCONFIG);

  if (g_file_query_exists (settings_file, cancellable))
    {
      if (!ide_buildconfig_configuration_provider_restore (self, settings_file, configs, cancellable, &error))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }
    }

  g_task_return_pointer (task, g_steal_pointer (&configs), (GDestroyNotify)g_ptr_array_unref);

  IDE_EXIT;
}

static void
ide_buildconfig_configuration_provider_load_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  IdeBuildconfigConfigurationProvider *self;
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (object));
  g_assert (G_IS_TASK (result));
  g_assert (G_IS_TASK (task));

  ar = g_task_propagate_pointer (G_TASK (result), &error);
  self = g_task_get_source_object (G_TASK (result));

  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));
  g_assert (self->configurations == NULL);

  if (ar != NULL && self->manager != NULL)
    {
      for (guint i = 0; i < ar->len; i++)
        {
          IdeConfiguration *config = g_ptr_array_index (ar, i);

          g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION (config));

          ide_configuration_manager_add (self->manager, config);

          if (g_object_get_data (G_OBJECT (config), "WAS_DEFAULT"))
            {
              ide_configuration_manager_set_current (self->manager, config);
              g_object_set_data (G_OBJECT (config), "WAS_DEFAULT", NULL);
            }
        }
    }

  self->configurations = g_steal_pointer (&ar);

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_buildconfig_configuration_provider_load_async (IdeConfigurationProvider *provider,
                                                   IdeConfigurationManager  *manager,
                                                   GCancellable             *cancellable,
                                                   GAsyncReadyCallback       callback,
                                                   gpointer                  user_data)
{
  IdeBuildconfigConfigurationProvider *self = (IdeBuildconfigConfigurationProvider *)provider;
  g_autoptr(GTask) parent_task = NULL;
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  dzl_set_weak_pointer (&self->manager, manager);

  /* This task is needed so the caller knows when the load finishes */
  parent_task = g_task_new (self, cancellable, callback, user_data);

  /* This task is used to run the load_worker in its own thread */
  task = g_task_new (self, cancellable, ide_buildconfig_configuration_provider_load_cb, g_steal_pointer (&parent_task));
  g_task_set_source_tag (task, ide_buildconfig_configuration_provider_load_async);
  g_task_set_task_data (task, g_object_ref (manager), g_object_unref);
  g_task_run_in_thread (task, ide_buildconfig_configuration_provider_load_worker);

  IDE_EXIT;
}

gboolean
ide_buildconfig_configuration_provider_load_finish (IdeConfigurationProvider  *provider,
                                                    GAsyncResult              *result,
                                                    GError                   **error)
{
  IdeBuildconfigConfigurationProvider *self = (IdeBuildconfigConfigurationProvider *)provider;

  g_return_val_if_fail (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_buildconfig_configuration_provider_unload (IdeConfigurationProvider *provider,
                                               IdeConfigurationManager  *manager)
{
  IdeBuildconfigConfigurationProvider *self = (IdeBuildconfigConfigurationProvider *)provider;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (manager));

  dzl_clear_source (&self->writeback_handler);

  if (self->configurations != NULL)
    {
      for (guint i= 0; i < self->configurations->len; i++)
        {
          IdeConfiguration *configuration = g_ptr_array_index (self->configurations, i);

          ide_configuration_manager_remove (manager, configuration);
        }
    }

  g_clear_pointer (&self->configurations, g_ptr_array_unref);

  dzl_clear_weak_pointer (&self->manager);

  IDE_EXIT;
}

void
ide_buildconfig_configuration_provider_track_config (IdeBuildconfigConfigurationProvider *self,
                                                     IdeBuildconfigConfiguration         *config)
{
  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (self));
  g_assert (IDE_IS_BUILDCONFIG_CONFIGURATION (config));

  g_signal_connect_object (config,
                           "changed",
                           G_CALLBACK (ide_buildconfig_configuration_provider_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_ptr_array_add (self->configurations, g_object_ref (config));
}

static void
ide_buildconfig_configuration_provider_class_init (IdeBuildconfigConfigurationProviderClass *klass)
{
}

static void
ide_buildconfig_configuration_provider_init (IdeBuildconfigConfigurationProvider *self)
{
}

static void
configuration_provider_iface_init (IdeConfigurationProviderInterface *iface)
{
  iface->load_async = ide_buildconfig_configuration_provider_load_async;
  iface->load_finish = ide_buildconfig_configuration_provider_load_finish;
  iface->unload = ide_buildconfig_configuration_provider_unload;
  iface->save_async = ide_buildconfig_configuration_provider_save_async;
  iface->save_finish = ide_buildconfig_configuration_provider_save_finish;
}
