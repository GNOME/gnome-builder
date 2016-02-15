/* ide-configuration-manager.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-configuration-manager"

#include <glib/gi18n.h>

#include "ide-configuration.h"
#include "ide-configuration-manager.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-environment.h"
#include "ide-macros.h"
#include "ide-vcs.h"

#define DOT_BUILD_CONFIG ".buildconfig"
#define WRITEBACK_TIMEOUT_SECS 2

struct _IdeConfigurationManager
{
  GObject           parent_instance;

  GPtrArray        *configurations;
  IdeConfiguration *current;
  GKeyFile         *key_file;

  gulong            writeback_handler;
  guint             change_count;
};

static void async_initable_iface_init (GAsyncInitableIface *iface);
static void list_model_iface_init     (GListModelInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeConfigurationManager, ide_configuration_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

enum {
  PROP_0,
  PROP_CURRENT,
  PROP_CURRENT_DISPLAY_NAME,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

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
ide_configuration_manager_load (IdeConfigurationManager  *self,
                                GKeyFile                 *key_file,
                                const gchar              *group,
                                GError                  **error)
{
  g_autoptr(IdeConfiguration) configuration = NULL;
  g_autofree gchar *env_group = NULL;
  IdeContext *context;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (key_file != NULL);
  g_assert (group != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));

  configuration = g_object_new (IDE_TYPE_CONFIGURATION,
                                "id", group,
                                "context", context,
                                NULL);

  load_string (configuration, key_file, group, "config-opts", "config-opts");
  load_string (configuration, key_file, group, "device", "device-id");
  load_string (configuration, key_file, group, "name", "display-name");
  load_string (configuration, key_file, group, "runtime", "runtime-id");
  load_string (configuration, key_file, group, "prefix", "prefix");

  env_group = g_strdup_printf ("%s.environment", group);

  if (g_key_file_has_group (key_file, env_group))
    load_environ (configuration, key_file, env_group);

  ide_configuration_set_dirty (configuration, FALSE);

  ide_configuration_manager_add (self, configuration);

  if (g_key_file_get_boolean (key_file, group, "default", NULL))
    ide_configuration_manager_set_current (self, configuration);

  return TRUE;
}

static gboolean
ide_configuration_manager_restore (IdeConfigurationManager  *self,
                                   GFile                    *file,
                                   GCancellable             *cancellable,
                                   GError                  **error)
{
  g_autofree gchar *contents = NULL;
  g_auto(GStrv) groups = NULL;
  gsize length = 0;
  guint i;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
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

      if (!ide_configuration_manager_load (self, self->key_file, groups [i], error))
        IDE_RETURN (FALSE);
    }

  IDE_RETURN (TRUE);
}

static void
ide_configuration_manager_save_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  GFile *file = (GFile *)object;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

void
ide_configuration_manager_save_async (IdeConfigurationManager *self,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data)
{
  g_autoptr(GHashTable) group_names = NULL;
  g_autoptr(GTask) task = NULL;
  g_auto(GStrv) groups = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GBytes) bytes = NULL;
  gchar *data;
  gsize length;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  GError *error = NULL;
  guint i;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  if (self->change_count == 0)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  self->change_count = 0;

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  file = g_file_get_child (workdir, DOT_BUILD_CONFIG);

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

  for (i = 0; i < self->configurations->len; i++)
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
                             ide_configuration_##getter (configuration))
      PERSIST_STRING_KEY ("name", get_display_name);
      PERSIST_STRING_KEY ("device", get_device_id);
      PERSIST_STRING_KEY ("runtime", get_runtime_id);
      PERSIST_STRING_KEY ("config-opts", get_config_opts);
      PERSIST_STRING_KEY ("prefix", get_prefix);
#undef PERSIST_STRING_KEY

      if (configuration == self->current)
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

          if (!ide_str_empty0 (key))
            g_key_file_set_string (self->key_file, group_environ, key, value ?: "");
        }
    }

  /*
   * Now truncate any old groups in the keyfile.
   */
  if (NULL != (groups = g_key_file_get_groups (self->key_file, NULL)))
    {
      for (i = 0; groups [i]; i++)
        {
          if (!g_hash_table_contains (group_names, groups [i]))
            g_key_file_remove_group (self->key_file, groups [i], NULL);
        }
    }

  if (NULL == (data = g_key_file_to_data (self->key_file, &length, &error)))
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  bytes = g_bytes_new_take (data, length);

  g_file_replace_contents_bytes_async (file,
                                       bytes,
                                       NULL,
                                       FALSE,
                                       G_FILE_CREATE_NONE,
                                       cancellable,
                                       ide_configuration_manager_save_cb,
                                       g_object_ref (task));

  IDE_EXIT;
}

gboolean
ide_configuration_manager_save_finish (IdeConfigurationManager  *self,
                                       GAsyncResult             *result,
                                       GError                  **error)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
ide_configuration_manager_do_writeback (gpointer data)
{
  IdeConfigurationManager *self = data;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));

  self->writeback_handler = 0;

  ide_configuration_manager_save_async (self, NULL, NULL, NULL);

  return G_SOURCE_REMOVE;
}

static void
ide_configuration_manager_queue_writeback (IdeConfigurationManager *self)
{
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));

  if (self->writeback_handler != 0)
    g_source_remove (self->writeback_handler);

  self->writeback_handler = g_timeout_add_seconds (WRITEBACK_TIMEOUT_SECS,
                                                   ide_configuration_manager_do_writeback,
                                                   self);
}

static void
ide_configuration_manager_add_default (IdeConfigurationManager *self)
{
  g_autoptr(IdeConfiguration) config = NULL;
  IdeContext *context;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  config = ide_configuration_new (context, "default", "local", "host");
  ide_configuration_set_display_name (config, _("Default Configuration"));
  ide_configuration_manager_add (self, config);

  if (self->configurations->len == 1)
    ide_configuration_manager_set_current (self, config);
}

/**
 * ide_configuration_manager_get_configuration:
 * @self: An #IdeConfigurationManager
 * @id: The string identifier of the configuration
 *
 * Gets the #IdeConfiguration by id. See ide_configuration_get_id().
 *
 * Returns: (transfer none) (nullable): An #IdeConfiguration or %NULL if
 *   the configuration could not be found.
 */
IdeConfiguration *
ide_configuration_manager_get_configuration (IdeConfigurationManager *self,
                                             const gchar             *id)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION_MANAGER (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  for (guint i = 0; i < self->configurations->len; i++)
    {
      IdeConfiguration *configuration = g_ptr_array_index (self->configurations, i);

      if (g_strcmp0 (id, ide_configuration_get_id (configuration)) == 0)
        return configuration;
    }

  return NULL;
}

static void
ide_configuration_manager_finalize (GObject *object)
{
  IdeConfigurationManager *self = (IdeConfigurationManager *)object;

  ide_clear_source (&self->writeback_handler);
  g_clear_pointer (&self->configurations, g_ptr_array_unref);
  g_clear_pointer (&self->key_file, g_key_file_free);
  g_clear_object (&self->current);

  G_OBJECT_CLASS (ide_configuration_manager_parent_class)->finalize (object);
}

static void
ide_configuration_manager_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeConfigurationManager *self = IDE_CONFIGURATION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_CURRENT:
      g_value_set_object (value, ide_configuration_manager_get_current (self));
      break;

    case PROP_CURRENT_DISPLAY_NAME:
      {
        IdeConfiguration *current = ide_configuration_manager_get_current (self);
        g_value_set_string (value, ide_configuration_get_display_name (current));
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_configuration_manager_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeConfigurationManager *self = IDE_CONFIGURATION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_CURRENT:
      ide_configuration_manager_set_current (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_configuration_manager_class_init (IdeConfigurationManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_configuration_manager_finalize;
  object_class->get_property = ide_configuration_manager_get_property;
  object_class->set_property = ide_configuration_manager_set_property;

  properties [PROP_CURRENT] =
    g_param_spec_object ("current",
                         "Current",
                         "The current configuration for the context",
                         IDE_TYPE_CONFIGURATION,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CURRENT_DISPLAY_NAME] =
    g_param_spec_string ("current-display-name",
                         "Current Display Name",
                         "The display name of the current configuration",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_configuration_manager_init (IdeConfigurationManager *self)
{
  self->configurations = g_ptr_array_new_with_free_func (g_object_unref);
}

static GType
ide_configuration_manager_get_item_type (GListModel *model)
{
  return IDE_TYPE_CONFIGURATION;
}

static guint
ide_configuration_manager_get_n_items (GListModel *model)
{
  IdeConfigurationManager *self = (IdeConfigurationManager *)model;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));

  return self->configurations->len;
}

static gpointer
ide_configuration_manager_get_item (GListModel *model,
                                    guint       position)
{
  IdeConfigurationManager *self = (IdeConfigurationManager *)model;

  g_return_val_if_fail (IDE_IS_CONFIGURATION_MANAGER (self), NULL);
  g_return_val_if_fail (position < self->configurations->len, NULL);

  return g_object_ref (g_ptr_array_index (self->configurations, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_configuration_manager_get_item_type;
  iface->get_n_items = ide_configuration_manager_get_n_items;
  iface->get_item = ide_configuration_manager_get_item;
}

static void
ide_configuration_manager_init_worker (GTask        *task,
                                       gpointer      source_object,
                                       gpointer      task_data,
                                       GCancellable *cancellable)
{
  IdeConfigurationManager *self = source_object;
  g_autoptr(GFile) settings_file = NULL;
  IdeContext *context;
  GError *error = NULL;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  settings_file = g_file_get_child (workdir, DOT_BUILD_CONFIG);

  if (!g_file_query_exists (settings_file, cancellable) ||
      !ide_configuration_manager_restore (self, settings_file, cancellable, &error))
    ide_configuration_manager_add_default (self);

  g_task_return_boolean (task, TRUE);
}

static void
ide_configuration_manager_init_async (GAsyncInitable      *initable,
                                      gint                 priority,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  IdeConfigurationManager *self = (IdeConfigurationManager *)initable;
  g_autoptr(GTask) task = NULL;

  g_assert (G_IS_ASYNC_INITABLE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, ide_configuration_manager_init_worker);
}

static gboolean
ide_configuration_manager_init_finish (GAsyncInitable  *initable,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  g_assert (IDE_IS_CONFIGURATION_MANAGER (initable));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_configuration_manager_init_async;
  iface->init_finish = ide_configuration_manager_init_finish;
}

void
ide_configuration_manager_set_current (IdeConfigurationManager *self,
                                       IdeConfiguration        *current)
{
  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (self));
  g_return_if_fail (!current || IDE_IS_CONFIGURATION (current));

  if (g_set_object (&self->current, current))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_DISPLAY_NAME]);
    }
}

/**
 * ide_configuration_manager_get_current:
 * @self: An #IdeConfigurationManager
 *
 * Gets the current configuration to use for building.
 *
 * Many systems allow you to pass a configuration in instead of relying on the
 * default configuration. This sets the default configuration that various
 * background items might use, such as tags builders which need to discover
 * settings.
 *
 * Returns: (transfer none): An #IdeConfiguration
 */
IdeConfiguration *
ide_configuration_manager_get_current (IdeConfigurationManager *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION_MANAGER (self), NULL);

  if ((self->current == NULL) && (self->configurations->len > 0))
    return g_ptr_array_index (self->configurations, 0);

  return self->current;
}

static void
ide_configuration_manager_changed (IdeConfigurationManager *self,
                                   GParamSpec              *pspec,
                                   IdeConfiguration        *configuration)
{
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  self->change_count++;

  ide_configuration_manager_queue_writeback (self);
}

void
ide_configuration_manager_add (IdeConfigurationManager *self,
                               IdeConfiguration        *configuration)
{
  guint position;

  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (configuration));

  g_signal_connect_object (configuration,
                           "changed",
                           G_CALLBACK (ide_configuration_manager_changed),
                           self,
                           G_CONNECT_SWAPPED);

  position = self->configurations->len;
  g_ptr_array_add (self->configurations, g_object_ref (configuration));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

void
ide_configuration_manager_remove (IdeConfigurationManager *self,
                                  IdeConfiguration        *configuration)
{
  guint i;

  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (configuration));

  for (i = 0; i < self->configurations->len; i++)
    {
      IdeConfiguration *item = g_ptr_array_index (self->configurations, i);

      if (item == configuration)
        {
          g_ptr_array_remove_index (self->configurations, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          if (self->configurations->len == 0)
            ide_configuration_manager_add_default (self);
          if (self->current == configuration)
            ide_configuration_manager_set_current (self, NULL);
          break;
        }
    }
}
