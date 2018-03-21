/* ide-configuration-manager.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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
#include <libpeas/peas.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "application/ide-application.h"
#include "config/ide-configuration-manager.h"
#include "config/ide-configuration.h"
#include "config/ide-configuration-provider.h"
#include "buildconfig/ide-buildconfig-configuration.h"
#include "buildconfig/ide-buildconfig-configuration-provider.h"
#include "threading/ide-task.h"

#define WRITEBACK_DELAY_SEC 3

struct _IdeConfigurationManager
{
  GObject           parent_instance;

  GCancellable     *cancellable;
  GArray           *configs;
  IdeConfiguration *current;
  PeasExtensionSet *providers;
  GSettings        *project_settings;

  guint             queued_save_source;

  guint             propagate_to_settings : 1;
};

typedef struct
{
  IdeConfigurationProvider *provider;
  IdeConfiguration         *config;
} ConfigInfo;

static void async_initable_iface_init           (GAsyncInitableIface *iface);
static void list_model_iface_init               (GListModelInterface *iface);
static void ide_configuration_manager_save_tick (IdeTask             *task);

G_DEFINE_TYPE_EXTENDED (IdeConfigurationManager, ide_configuration_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

enum {
  PROP_0,
  PROP_CURRENT,
  PROP_CURRENT_DISPLAY_NAME,
  PROP_READY,
  LAST_PROP
};

enum {
  INVALIDATE,
  N_SIGNALS
};

static GParamSpec *properties [LAST_PROP];
static guint signals [N_SIGNALS];

static void
config_info_clear (gpointer data)
{
  ConfigInfo *info = data;

  g_clear_object (&info->config);
  g_clear_object (&info->provider);
}

static void
ide_configuration_manager_collect_providers (PeasExtensionSet *set,
                                             PeasPluginInfo   *plugin_info,
                                             PeasExtension    *exten,
                                             gpointer          user_data)
{
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)exten;
  GPtrArray *providers = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));
  g_assert (providers != NULL);

  g_ptr_array_add (providers, g_object_ref (provider));
}

static void
ide_configuration_manager_save_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (task));

  if (!ide_configuration_provider_save_finish (provider, result, &error))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (provider), error->message);

  ide_configuration_manager_save_tick (task);

  IDE_EXIT;
}

static void
ide_configuration_manager_save_tick (IdeTask *task)
{
  IdeConfigurationProvider *provider;
  GCancellable *cancellable;
  GPtrArray *providers;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));

  providers = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  if (providers->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  provider = g_ptr_array_index (providers, providers->len - 1);

  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));

  ide_configuration_provider_save_async (provider,
                                         cancellable,
                                         ide_configuration_manager_save_cb,
                                         g_object_ref (task));

  g_ptr_array_remove_index (providers, providers->len - 1);

  IDE_EXIT;
}

void
ide_configuration_manager_save_async (IdeConfigurationManager *self,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data)
{
  g_autoptr(GPtrArray) providers = NULL;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_configuration_manager_save_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  providers = g_ptr_array_new_with_free_func (g_object_unref);
  peas_extension_set_foreach (self->providers,
                              ide_configuration_manager_collect_providers,
                              providers);
  ide_task_set_task_data (task, g_ptr_array_ref (providers), (GDestroyNotify)g_ptr_array_unref);

  if (providers->len == 0)
    ide_task_return_boolean (task, TRUE);
  else
    ide_configuration_manager_save_tick (task);

  IDE_EXIT;
}

gboolean
ide_configuration_manager_save_finish (IdeConfigurationManager  *self,
                                       GAsyncResult             *result,
                                       GError                  **error)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
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

  for (guint i = 0; i < self->configs->len; i++)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, i);

      g_assert (IDE_IS_CONFIGURATION (info->config));

      if (dzl_str_equal0 (id, ide_configuration_get_id (info->config)))
        return info->config;
    }

  return NULL;
}

static const gchar *
ide_configuration_manager_get_display_name (IdeConfigurationManager *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION_MANAGER (self), NULL);

  if (self->current != NULL)
    return ide_configuration_get_display_name (self->current);

  return "";
}

static void
ide_configuration_manager_notify_display_name (IdeConfigurationManager *self,
                                               GParamSpec              *pspec,
                                               IdeConfiguration        *configuration)
{
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_DISPLAY_NAME]);
}

static void
ide_configuration_manager_notify_ready (IdeConfigurationManager *self,
                                        GParamSpec              *pspec,
                                        IdeConfiguration        *configuration)
{
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READY]);
}

static void
ide_configuration_manager_dispose (GObject *object)
{
  IdeConfigurationManager *self = (IdeConfigurationManager *)object;

  if (self->current != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->current,
                                            G_CALLBACK (ide_configuration_manager_notify_display_name),
                                            self);
      g_signal_handlers_disconnect_by_func (self->current,
                                            G_CALLBACK (ide_configuration_manager_notify_ready),
                                            self);
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->project_settings);

  G_OBJECT_CLASS (ide_configuration_manager_parent_class)->dispose (object);
}

static void
ide_configuration_manager_finalize (GObject *object)
{
  IdeConfigurationManager *self = (IdeConfigurationManager *)object;

  g_clear_object (&self->current);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->configs, g_array_unref);

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
      g_value_set_string (value, ide_configuration_manager_get_display_name (self));
      break;

    case PROP_READY:
      g_value_set_boolean (value, ide_configuration_manager_get_ready (self));
      break;

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

  object_class->dispose = ide_configuration_manager_dispose;
  object_class->finalize = ide_configuration_manager_finalize;
  object_class->get_property = ide_configuration_manager_get_property;
  object_class->set_property = ide_configuration_manager_set_property;

  properties [PROP_CURRENT] =
    g_param_spec_object ("current",
                         "Current",
                         "The current configuration for the context",
                         IDE_TYPE_CONFIGURATION,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CURRENT_DISPLAY_NAME] =
    g_param_spec_string ("current-display-name",
                         "Current Display Name",
                         "The display name of the current configuration",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_READY] =
    g_param_spec_boolean ("ready",
                          "Ready",
                          "If the current configuration is ready",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * IdeConfigurationManager::invalidate:
   * @self: an #IdeConfigurationManager
   *
   * This signal is emitted any time a new configuration is selected or the
   * currently selected configurations state changes.
   */
  signals [INVALIDATE] =
    g_signal_new ("invalidate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
ide_configuration_manager_init (IdeConfigurationManager *self)
{
  self->cancellable = g_cancellable_new ();
  self->configs = g_array_new (FALSE, FALSE, sizeof (ConfigInfo));
  g_array_set_clear_func (self->configs, config_info_clear);
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
  g_assert (self->configs != NULL);

  return self->configs->len;
}

static gpointer
ide_configuration_manager_get_item (GListModel *model,
                                    guint       position)
{
  IdeConfigurationManager *self = (IdeConfigurationManager *)model;
  const ConfigInfo *info;

  g_return_val_if_fail (IDE_IS_CONFIGURATION_MANAGER (self), NULL);
  g_return_val_if_fail (position < self->configs->len, NULL);

  info = &g_array_index (self->configs, ConfigInfo, position);

  return g_object_ref (info->config);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_configuration_manager_get_item_type;
  iface->get_n_items = ide_configuration_manager_get_n_items;
  iface->get_item = ide_configuration_manager_get_item;
}

static gboolean
ide_configuration_manager_do_save (gpointer data)
{
  IdeConfigurationManager *self = data;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));

  self->queued_save_source = 0;

  g_signal_emit (self, signals [INVALIDATE], 0);

  ide_configuration_manager_save_async (self, NULL, NULL, NULL);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_configuration_manager_changed (IdeConfigurationManager *self,
                                   IdeConfiguration        *config)
{
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_CONFIGURATION (config));

  dzl_clear_source (&self->queued_save_source);
  self->queued_save_source =
    g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                WRITEBACK_DELAY_SEC,
                                ide_configuration_manager_do_save,
                                g_object_ref (self),
                                g_object_unref);
}

static void
ide_configuration_manager_config_added (IdeConfigurationManager  *self,
                                        IdeConfiguration         *config,
                                        IdeConfigurationProvider *provider)
{
  ConfigInfo info = {0};

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_CONFIGURATION (config));
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));

  g_signal_connect_object (config,
                           "changed",
                           G_CALLBACK (ide_configuration_manager_changed),
                           self,
                           G_CONNECT_SWAPPED);

  info.provider = g_object_ref (provider);
  info.config = g_object_ref (config);
  g_array_append_val (self->configs, info);

  g_list_model_items_changed (G_LIST_MODEL (self), self->configs->len - 1, 0, 1);

  if (self->current == NULL)
    ide_configuration_manager_set_current (self, config);

  IDE_EXIT;
}

static void
ide_configuration_manager_config_removed (IdeConfigurationManager  *self,
                                          IdeConfiguration         *config,
                                          IdeConfigurationProvider *provider)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_CONFIGURATION (config));
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));

  for (guint i = 0; i < self->configs->len; i++)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, i);

      if (info->provider == provider && info->config == config)
        {
          g_signal_handlers_disconnect_by_func (config,
                                                G_CALLBACK (ide_configuration_manager_changed),
                                                self);
          g_array_remove_index (self->configs, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          break;
        }
    }

  IDE_EXIT;
}

static void
ide_configuration_manager_provider_load_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)object;
  IdeContext *context;
  g_autoptr(IdeConfigurationManager) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_TASK (result));

  context = ide_object_get_context (IDE_OBJECT (self));

  if (!ide_configuration_provider_load_finish (provider, result, &error))
    ide_context_warning (context,
                         "Failed to initialize config provider: %s: %s",
                         G_OBJECT_TYPE_NAME (provider), error->message);

  IDE_EXIT;
}

static void
provider_connect (IdeConfigurationManager  *self,
                  IdeConfigurationProvider *provider)
{
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));

  g_signal_connect_object (provider,
                           "added",
                           G_CALLBACK (ide_configuration_manager_config_added),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (provider,
                           "removed",
                           G_CALLBACK (ide_configuration_manager_config_removed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
provider_disconnect (IdeConfigurationManager  *self,
                     IdeConfigurationProvider *provider)
{
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_configuration_manager_config_added),
                                        self);
  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_configuration_manager_config_removed),
                                        self);
}

static void
ide_configuration_manager_provider_added (PeasExtensionSet *set,
                                          PeasPluginInfo   *plugin_info,
                                          PeasExtension    *exten,
                                          gpointer          user_data)
{
  IdeConfigurationManager *self = user_data;
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)exten;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));

  provider_connect (self, provider);

  ide_configuration_provider_load_async (provider,
                                         self->cancellable,
                                         ide_configuration_manager_provider_load_cb,
                                         g_object_ref (self));
}

static void
ide_configuration_manager_provider_removed (PeasExtensionSet *set,
                                            PeasPluginInfo   *plugin_info,
                                            PeasExtension    *exten,
                                            gpointer          user_data)
{
  IdeConfigurationManager *self = user_data;
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)exten;
  g_autoptr(IdeConfigurationProvider) hold = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));

  hold = g_object_ref (provider);

  ide_configuration_provider_unload (provider);

  provider_disconnect (self, provider);

  for (guint i = self->configs->len; i > 0; i--)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, i - 1);

      if (info->provider == provider)
        {
          g_warning ("%s failed to remove configuration \"%s\"",
                     G_OBJECT_TYPE_NAME (provider),
                     ide_configuration_get_id (info->config));
          g_array_remove_index (self->configs, i);
        }
    }
}

static void
notify_providers_loaded (IdeConfigurationManager *self,
                         GParamSpec              *pspec,
                         IdeTask                 *task)
{
  g_autoptr(GVariant) user_value = NULL;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_TASK (task));

  if (self->project_settings == NULL)
    return;

  /*
   * At this point, all of our configuratin providers have returned from
   * their asynchronous loading. So we should have all of the configs we
   * can know about at this point.
   *
   * We need to read our config-id from project_settings, and if we find
   * a match, make that our active configuration.
   *
   * We want to avoid applying the value if the value is unchanged
   * according to g_settings_get_user_value() so that we don't override
   * any provider that set_current() during it's load, unless the user
   * has manually set this config in the past.
   *
   * Once we have updated the current config, we can start propagating
   * new values to the settings when set_current() is called.
   */

  user_value = g_settings_get_user_value (self->project_settings, "config-id");

  if (user_value != NULL)
    {
      const gchar *str = g_variant_get_string (user_value, NULL);
      IdeConfiguration *config;

      if ((config = ide_configuration_manager_get_configuration (self, str)))
        {
          if (config != self->current)
            ide_configuration_manager_set_current (self, config);
        }
    }

  self->propagate_to_settings = TRUE;
}

static void
ide_configuration_manager_init_load_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)object;
  IdeConfigurationManager *self;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  GPtrArray *providers;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  if (!ide_configuration_provider_load_finish (provider, result, &error))
    {
      g_print ("%s\n", G_OBJECT_TYPE_NAME (provider));
      g_assert (error != NULL);
      ide_context_warning (context,
                           "Failed to initialize config provider: %s: %s",
                           G_OBJECT_TYPE_NAME (provider), error->message);
    }

  providers = ide_task_get_task_data (task);
  g_assert (providers != NULL);
  g_assert (providers->len > 0);

  if (!g_ptr_array_remove (providers, provider))
    g_critical ("Failed to locate provider in active set");

  if (providers->len == 0)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_configuration_manager_init_async (GAsyncInitable      *initable,
                                      gint                 priority,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  IdeConfigurationManager *self = (IdeConfigurationManager *)initable;
  g_autoptr(GPtrArray) providers = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdeContext *context;

  g_assert (G_IS_ASYNC_INITABLE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_configuration_manager_init_async);
  ide_task_set_priority (task, priority);

  g_signal_connect_swapped (task,
                            "notify::completed",
                            G_CALLBACK (notify_providers_loaded),
                            self);

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  self->project_settings = ide_context_get_project_settings (context);

  self->providers = peas_extension_set_new (peas_engine_get_default (),
                                            IDE_TYPE_CONFIGURATION_PROVIDER,
                                            "context", context,
                                            NULL);

  g_signal_connect (self->providers,
                    "extension-added",
                    G_CALLBACK (ide_configuration_manager_provider_added),
                    self);

  g_signal_connect (self->providers,
                    "extension-removed",
                    G_CALLBACK (ide_configuration_manager_provider_removed),
                    self);

  providers = g_ptr_array_new_with_free_func (g_object_unref);
  peas_extension_set_foreach (self->providers,
                              ide_configuration_manager_collect_providers,
                              providers);
  ide_task_set_task_data (task, g_ptr_array_ref (providers), (GDestroyNotify)g_ptr_array_unref);

  for (guint i = 0; i < providers->len; i++)
    {
      IdeConfigurationProvider *provider = g_ptr_array_index (providers, i);

      g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));

      provider_connect (self, provider);

      ide_configuration_provider_load_async (provider,
                                             cancellable,
                                             ide_configuration_manager_init_load_cb,
                                             g_object_ref (task));
    }

  if (providers->len == 0)
    ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_configuration_manager_init_finish (GAsyncInitable  *initable,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIGURATION_MANAGER (initable));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
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
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (self));
  g_return_if_fail (!current || IDE_IS_CONFIGURATION (current));

  if (self->current != current)
    {
      if (self->current != NULL)
        {
          g_signal_handlers_disconnect_by_func (self->current,
                                                G_CALLBACK (ide_configuration_manager_notify_display_name),
                                                self);
          g_signal_handlers_disconnect_by_func (self->current,
                                                G_CALLBACK (ide_configuration_manager_notify_ready),
                                                self);
          g_clear_object (&self->current);
        }

      if (current != NULL)
        {
          self->current = g_object_ref (current);

          g_signal_connect_object (current,
                                   "notify::display-name",
                                   G_CALLBACK (ide_configuration_manager_notify_display_name),
                                   self,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (current,
                                   "notify::ready",
                                   G_CALLBACK (ide_configuration_manager_notify_ready),
                                   self,
                                   G_CONNECT_SWAPPED);

          if (self->propagate_to_settings && self->project_settings != NULL)
            {
              g_autofree gchar *new_id = g_strdup (ide_configuration_get_id (current));
              g_settings_set_string (self->project_settings, "config-id", new_id);
            }
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_DISPLAY_NAME]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READY]);

      g_signal_emit (self, signals [INVALIDATE], 0);
    }
}

/**
 * ide_configuration_manager_get_current:
 * @self: An #IdeConfigurationManager
 *
 * Gets the current configuration to use for building.
 *
 * Many systems allow you to pass a configuration in instead of relying on the
 * default configuration. This gets the default configuration that various
 * background items might use, such as tags builders which need to discover
 * settings.
 *
 * Returns: (transfer none): An #IdeConfiguration
 */
IdeConfiguration *
ide_configuration_manager_get_current (IdeConfigurationManager *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION_MANAGER (self), NULL);
  g_return_val_if_fail (self->current != NULL || self->configs->len > 0, NULL);

  if (self->current != NULL)
    return self->current;

  if (self->configs->len > 0)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, 0);

      g_assert (IDE_IS_CONFIGURATION_PROVIDER (info->provider));
      g_assert (IDE_IS_CONFIGURATION (info->config));

      return info->config;
    }

  g_critical ("Failed to locate activate configuration. This should not happen.");

  return NULL;
}

void
ide_configuration_manager_duplicate (IdeConfigurationManager *self,
                                     IdeConfiguration        *config)
{
  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (config));

  for (guint i = 0; i < self->configs->len; i++)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, i);

      g_assert (IDE_IS_CONFIGURATION_PROVIDER (info->provider));
      g_assert (IDE_IS_CONFIGURATION (info->config));

      if (info->config == config)
        {
          g_autoptr(IdeConfigurationProvider) provider = g_object_ref (info->provider);

          info = NULL; /* info becomes invalid */
          ide_configuration_provider_duplicate (provider, config);
          ide_configuration_provider_save_async (provider, NULL, NULL, NULL);
          break;
        }
    }
}

void
ide_configuration_manager_delete (IdeConfigurationManager *self,
                                  IdeConfiguration        *config)
{
  g_autoptr(IdeConfiguration) hold = NULL;

  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (config));

  hold = g_object_ref (config);

  for (guint i = 0; i < self->configs->len; i++)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, i);
      g_autoptr(IdeConfigurationProvider) provider = NULL;

      g_assert (IDE_IS_CONFIGURATION_PROVIDER (info->provider));
      g_assert (IDE_IS_CONFIGURATION (info->config));

      provider = g_object_ref (info->provider);

      if (info->config == config)
        {
          info = NULL; /* info becomes invalid */
          ide_configuration_provider_delete (provider, config);
          ide_configuration_provider_save_async (provider, NULL, NULL, NULL);
          break;
        }
    }
}

/**
 * ide_configuration_manager_get_ready:
 * @self: an #IdeConfigurationManager
 *
 * This returns %TRUE if the current configuration is ready for usage.
 *
 * This is equivalent to checking the ready property of the current
 * configuration. It allows consumers to not need to track changes to
 * the current configuration.
 *
 * Returns: %TRUE if the current configuration is ready for usage;
 *   otherwise %FALSE.
 *
 * Since: 3.28
 */
gboolean
ide_configuration_manager_get_ready (IdeConfigurationManager *self)
{
  IdeConfiguration *config;

  g_return_val_if_fail (IDE_IS_CONFIGURATION_MANAGER (self), FALSE);

  if ((config = ide_configuration_manager_get_current (self)))
    return ide_configuration_get_ready (config);

  return FALSE;
}
