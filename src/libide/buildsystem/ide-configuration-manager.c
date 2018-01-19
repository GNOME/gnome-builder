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

#include "buildsystem/ide-configuration-manager.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-configuration-provider.h"

#include "buildconfig/ide-buildconfig-configuration.h"
#include "buildconfig/ide-buildconfig-configuration-provider.h"

struct _IdeConfigurationManager
{
  GObject           parent_instance;

  GPtrArray        *configurations;
  IdeConfiguration *current;
  PeasExtensionSet *extensions;
  GCancellable     *cancellable;
  guint             providers_loading;
};

static void async_initable_iface_init           (GAsyncInitableIface *iface);
static void list_model_iface_init               (GListModelInterface *iface);
static void ide_configuration_manager_save_tick (GTask               *task);

G_DEFINE_TYPE_EXTENDED (IdeConfigurationManager, ide_configuration_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

enum {
  PROP_0,
  PROP_CURRENT,
  PROP_CURRENT_DISPLAY_NAME,
  LAST_PROP
};

enum {
  INVALIDATE,
  N_SIGNALS
};

static GParamSpec *properties [LAST_PROP];
static guint signals [N_SIGNALS];

static void
ide_configuration_manager_add_default (IdeConfigurationManager *self)
{
  g_autoptr(IdeBuildconfigConfiguration) config = NULL;
  IdeContext *context;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  config = g_object_new (IDE_TYPE_BUILDCONFIG_CONFIGURATION,
                         "id", "default",
                         "context", context,
                         "device-id", "local",
                         "runtime-id", "host",
                         NULL);
  ide_configuration_set_display_name (IDE_CONFIGURATION (config), _("Default"));
  ide_configuration_manager_add (self, IDE_CONFIGURATION (config));

  if (self->configurations->len == 1)
    ide_configuration_manager_set_current (self, IDE_CONFIGURATION (config));
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
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));
  g_assert (G_IS_TASK (task));

  if (!ide_configuration_provider_save_finish (provider, result, &error))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (provider), error->message);

  ide_configuration_manager_save_tick (task);

  IDE_EXIT;
}

static void
ide_configuration_manager_save_tick (GTask *task)
{
  IdeConfigurationProvider *provider;
  GCancellable *cancellable;
  GPtrArray *providers;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));

  providers = g_task_get_task_data (task);
  cancellable = g_task_get_cancellable (task);

  if (providers->len == 0)
    {
      g_task_return_boolean (task, TRUE);
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
  g_autoptr(GTask) task = NULL;
  g_autoptr(GPtrArray) providers = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_configuration_manager_save_async);

  providers = g_ptr_array_new_with_free_func (g_object_unref);

  peas_extension_set_foreach (self->extensions,
                              ide_configuration_manager_collect_providers,
                              providers);

  if (providers->len == 0)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  g_task_set_task_data (task, g_steal_pointer (&providers), (GDestroyNotify)g_ptr_array_unref);

  ide_configuration_manager_save_tick (task);

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
      IdeConfiguration *config = g_ptr_array_index (self->configurations, i);
      const gchar *config_id;

      g_assert (config != NULL);
      g_assert (IDE_IS_CONFIGURATION (config));

      config_id = ide_configuration_get_id (config);

      if (dzl_str_equal0 (config_id, id))
        return config;
    }

  return NULL;
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
ide_configuration_manager_finalize (GObject *object)
{
  IdeConfigurationManager *self = (IdeConfigurationManager *)object;

  g_clear_pointer (&self->configurations, g_ptr_array_unref);

  if (self->current != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->current,
                                            G_CALLBACK (ide_configuration_manager_notify_display_name),
                                            self);
      g_clear_object (&self->current);
    }

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

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

  /**
   * IdeConfigurationManager::invalidate:
   *
   * This signal is emitted any time a new configuration is selected or the
   * currently selected configurations state changes.
   */
  signals [INVALIDATE] =
    g_signal_new ("invalidate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
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
ide_configuration_manager_track_buildconfig (PeasExtensionSet *set,
                                             PeasPluginInfo   *plugin_info,
                                             PeasExtension    *exten,
                                             gpointer          user_data)
{
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)exten;
  IdeConfiguration *config = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));
  g_assert (!config || IDE_IS_BUILDCONFIG_CONFIGURATION (config));

  if (IDE_IS_BUILDCONFIG_CONFIGURATION_PROVIDER (provider) && config != NULL)
    ide_buildconfig_configuration_provider_track_config (IDE_BUILDCONFIG_CONFIGURATION_PROVIDER (provider),
                                                         IDE_BUILDCONFIG_CONFIGURATION (config));
}

static void
ide_configuration_manager_load_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)object;
  IdeConfigurationManager *self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (G_IS_TASK (result));

  if (!ide_configuration_provider_load_finish (provider, result, &error))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (provider), error->message);

  self->providers_loading--;

  if (self->providers_loading == 0)
    {
      IdeConfiguration *default_config;
      gboolean restored_buildconfig = FALSE;

      for (guint i = 0; i < self->configurations->len; i++)
        {
          IdeConfiguration *config = g_ptr_array_index (self->configurations, i);
          const gchar *config_id;

          g_assert (IDE_IS_CONFIGURATION (config));

          config_id = ide_configuration_get_id (config);

          if (IDE_IS_BUILDCONFIG_CONFIGURATION (config) && dzl_str_equal0 (config_id, "default"))
            restored_buildconfig = TRUE;
        }

      /*
       * If the default config was added by the manager rather than the provider,
       * let the provider know about it so changes are persisted to the disk.
       */
      default_config = ide_configuration_manager_get_configuration (self, "default");
      if (!restored_buildconfig)
        {
          if (default_config == NULL)
            {
              ide_configuration_manager_add_default (self);
              default_config = ide_configuration_manager_get_configuration (self, "default");
            }

          peas_extension_set_foreach (self->extensions,
                                      ide_configuration_manager_track_buildconfig,
                                      default_config);
        }
    }

  IDE_EXIT;
}

static void
ide_configuration_manager_extension_added (PeasExtensionSet *set,
                                           PeasPluginInfo   *plugin_info,
                                           PeasExtension    *exten,
                                           gpointer          user_data)
{
  IdeConfigurationManager *self = user_data;
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));

  self->providers_loading++;
  ide_configuration_provider_load_async (provider,
                                         self,
                                         self->cancellable,
                                         ide_configuration_manager_load_cb,
                                         self);
}

static void
ide_configuration_manager_extension_removed (PeasExtensionSet *set,
                                             PeasPluginInfo   *plugin_info,
                                             PeasExtension    *exten,
                                             gpointer          user_data)
{
  IdeConfigurationManager *self = user_data;
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));

  ide_configuration_provider_unload (provider, self);
}

static void
ide_configuration_manager_init_worker (GTask        *task,
                                       gpointer      source_object,
                                       gpointer      task_data,
                                       GCancellable *cancellable)
{
  IdeConfigurationManager *self = source_object;
  IdeContext *context;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  self->providers_loading = 0;

  self->cancellable = g_cancellable_new ();

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             IDE_TYPE_CONFIGURATION_PROVIDER,
                                             NULL);

  g_signal_connect (self->extensions,
                    "extension-added",
                    G_CALLBACK (ide_configuration_manager_extension_added),
                    self);

  g_signal_connect (self->extensions,
                    "extension-removed",
                    G_CALLBACK (ide_configuration_manager_extension_removed),
                    self);

  peas_extension_set_foreach (self->extensions,
                              ide_configuration_manager_extension_added,
                              self);

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

  if (self->current != current)
    {
      if (self->current != NULL)
        {
          g_signal_handlers_disconnect_by_func (self->current,
                                                G_CALLBACK (ide_configuration_manager_notify_display_name),
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
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_DISPLAY_NAME]);

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

  if ((self->current == NULL) && (self->configurations->len > 0))
    return g_ptr_array_index (self->configurations, 0);

  return self->current;
}

static void
ide_configuration_manager_changed (IdeConfigurationManager *self,
                                   IdeConfiguration        *configuration)
{
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  g_signal_emit (self, signals [INVALIDATE], 0);
}

void
ide_configuration_manager_add (IdeConfigurationManager *self,
                               IdeConfiguration        *configuration)
{
  const gchar *config_id;
  guint position;

  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (configuration));

  for (guint i = 0; i < self->configurations->len; i++)
    {
      IdeConfiguration *ele = g_ptr_array_index (self->configurations, i);

      /* Do nothing if we already have this. Unlikely to happen but might
       * be if we got into a weird race with registering default configurations
       * and receiving a default from a provider.
       */
      if (configuration == ele)
        return;
    }

  config_id = ide_configuration_get_id (configuration);

  /* Allow the default config to be overridden by one from a provider */
  if (dzl_str_equal0 ("default", config_id))
    {
      IdeConfiguration *def = ide_configuration_manager_get_configuration (self, "default");

      g_assert (def != configuration);

      if (def != NULL)
        g_ptr_array_remove_fast (self->configurations, def);
    }

  position = self->configurations->len;
  g_ptr_array_add (self->configurations, g_object_ref (configuration));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  g_signal_connect_object (configuration,
                           "changed",
                           G_CALLBACK (ide_configuration_manager_changed),
                           self,
                           G_CONNECT_SWAPPED);
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
          g_signal_handlers_disconnect_by_func (configuration,
                                                G_CALLBACK (ide_configuration_manager_changed),
                                                self);
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
