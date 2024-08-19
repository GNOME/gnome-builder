/* ide-config-manager.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-config-manager"

#include "config.h"

#include <glib/gi18n.h>

#include <libpeas.h>

#include <libide-threading.h>

#include "ide-marshal.h"

#include "ide-config.h"
#include "ide-config-manager.h"
#include "ide-config-private.h"
#include "ide-config-provider.h"
#include "ide-runtime.h"

#define WRITEBACK_DELAY_SEC 3

struct _IdeConfigManager
{
  IdeObject         parent_instance;

  GCancellable     *cancellable;
  GArray           *configs;
  IdeConfig        *current;
  PeasExtensionSet *providers;
  GSettings        *project_settings;

  GMenu            *menu;
  GMenu            *config_menu;

  guint             queued_save_source;

  guint             propagate_to_settings : 1;
  guint             save_needs_invalidate : 1;
};

typedef struct
{
  IdeConfigProvider *provider;
  IdeConfig         *config;
} ConfigInfo;

static void async_initable_iface_init            (GAsyncInitableIface *iface);
static void list_model_iface_init                (GListModelInterface *iface);
static void ide_config_manager_save_tick         (IdeTask             *task);
static void ide_config_manager_actions_current   (IdeConfigManager    *self,
                                                  GVariant            *param);
static void ide_config_manager_actions_delete    (IdeConfigManager    *self,
                                                  GVariant            *param);
static void ide_config_manager_actions_duplicate (IdeConfigManager    *self,
                                                  GVariant            *param);

IDE_DEFINE_ACTION_GROUP (IdeConfigManager, ide_config_manager, {
  { "current", ide_config_manager_actions_current, "s" },
  { "delete", ide_config_manager_actions_delete, "s" },
  { "duplicate", ide_config_manager_actions_duplicate, "s" },
})

G_DEFINE_TYPE_EXTENDED (IdeConfigManager, ide_config_manager, IDE_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, ide_config_manager_init_action_group))

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
ide_config_manager_actions_current (IdeConfigManager *self,
                                    GVariant         *param)
{
  IdeConfig *config;
  const gchar *id;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  id = g_variant_get_string (param, NULL);

  if ((config = ide_config_manager_get_config (self, id)))
    ide_config_manager_set_current (self, config);
}

static void
ide_config_manager_actions_duplicate (IdeConfigManager *self,
                                      GVariant         *param)
{
  IdeConfig *config;
  const gchar *id;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  id = g_variant_get_string (param, NULL);

  if ((config = ide_config_manager_get_config (self, id)))
    ide_config_manager_duplicate (self, config);
}

static void
ide_config_manager_actions_delete (IdeConfigManager *self,
                                   GVariant         *param)
{
  IdeConfig *config;
  const gchar *id;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  id = g_variant_get_string (param, NULL);

  if ((config = ide_config_manager_get_config (self, id)))
    ide_config_manager_delete (self, config);
}

static void
ide_config_manager_collect_providers (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      GObject    *exten,
                                      gpointer          user_data)
{
  IdeConfigProvider *provider = (IdeConfigProvider *)exten;
  GPtrArray *providers = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIG_PROVIDER (provider));
  g_assert (providers != NULL);

  g_ptr_array_add (providers, g_object_ref (provider));
}

static void
ide_config_manager_save_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeConfigProvider *provider = (IdeConfigProvider *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIG_PROVIDER (provider));
  g_assert (IDE_IS_TASK (task));

  if (!ide_config_provider_save_finish (provider, result, &error))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (provider), error->message);

  ide_config_manager_save_tick (task);

  IDE_EXIT;
}

static void
ide_config_manager_save_tick (IdeTask *task)
{
  IdeConfigProvider *provider;
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

  g_assert (IDE_IS_CONFIG_PROVIDER (provider));

  ide_config_provider_save_async (provider,
                                  cancellable,
                                  ide_config_manager_save_cb,
                                  g_object_ref (task));

  g_ptr_array_remove_index (providers, providers->len - 1);

  IDE_EXIT;
}

void
ide_config_manager_save_async (IdeConfigManager    *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(GPtrArray) providers = NULL;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_config_manager_save_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  providers = g_ptr_array_new_with_free_func (g_object_unref);
  peas_extension_set_foreach (self->providers,
                              ide_config_manager_collect_providers,
                              providers);
  ide_task_set_task_data (task, g_ptr_array_ref (providers), g_ptr_array_unref);

  if (providers->len == 0)
    ide_task_return_boolean (task, TRUE);
  else
    ide_config_manager_save_tick (task);

  IDE_EXIT;
}

gboolean
ide_config_manager_save_finish (IdeConfigManager  *self,
                                GAsyncResult      *result,
                                GError           **error)
{
  g_return_val_if_fail (IDE_IS_CONFIG_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

/**
 * ide_config_manager_get_config:
 * @self: An #IdeConfigManager
 * @id: The string identifier of the configuration
 *
 * Gets the #IdeConfig by id. See ide_config_get_id().
 *
 * Returns: (transfer none) (nullable): An #IdeConfig or %NULL if
 *   the configuration could not be found.
 */
IdeConfig *
ide_config_manager_get_config (IdeConfigManager *self,
                               const gchar      *id)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CONFIG_MANAGER (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  for (guint i = 0; i < self->configs->len; i++)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, i);

      g_assert (IDE_IS_CONFIG (info->config));

      if (ide_str_equal0 (id, ide_config_get_id (info->config)))
        return info->config;
    }

  return NULL;
}

static const gchar *
ide_config_manager_get_display_name (IdeConfigManager *self)
{
  g_return_val_if_fail (IDE_IS_CONFIG_MANAGER (self), NULL);

  if (self->current != NULL)
    return ide_config_get_display_name (self->current);

  return "";
}

static void
ide_config_manager_notify_display_name (IdeConfigManager *self,
                                        GParamSpec       *pspec,
                                        IdeConfig        *configuration)
{
  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (IDE_IS_CONFIG (configuration));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_DISPLAY_NAME]);
}

static void
ide_config_manager_notify_ready (IdeConfigManager *self,
                                 GParamSpec       *pspec,
                                 IdeConfig        *configuration)
{
  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (IDE_IS_CONFIG (configuration));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READY]);
}

static void
ide_config_manager_destroy (IdeObject *object)
{
  IdeConfigManager *self = (IdeConfigManager *)object;

  if (self->current != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->current,
                                            G_CALLBACK (ide_config_manager_notify_display_name),
                                            self);
      g_signal_handlers_disconnect_by_func (self->current,
                                            G_CALLBACK (ide_config_manager_notify_ready),
                                            self);
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->project_settings);

  IDE_OBJECT_CLASS (ide_config_manager_parent_class)->destroy (object);
}

static void
config_notify_cb (IdeConfig  *config,
                  GParamSpec *pspec,
                  GMenuItem  *item)
{
  g_assert (IDE_IS_CONFIG (config));
  g_assert (G_IS_MENU_ITEM (item));

  if (pspec == NULL || ide_str_equal0 (pspec->name, "display-name"))
    {
      const char *name = ide_config_get_display_name (config);
      g_menu_item_set_label (item, name);
    }

  if (pspec == NULL || ide_str_equal0 (pspec->name, "id"))
    {
      const char *id = ide_config_get_id (config);
      g_menu_item_set_action_and_target (item,
                                         "context.config-manager.current",
                                         "s", id);
    }
}

static void
items_changed_cb (IdeConfigManager *self,
                  guint             position,
                  guint             removed,
                  guint             added)
{
  g_assert (IDE_IS_CONFIG_MANAGER (self));

  for (guint i = 0; i < removed; i++)
    g_menu_remove (self->config_menu, position + i);

  for (guint i = 0; i < added; i++)
    {
      g_autoptr(IdeConfig) config = g_list_model_get_item (G_LIST_MODEL (self), position + i);
      g_autoptr(GMenuItem) item = g_menu_item_new (NULL, NULL);

      g_menu_item_set_attribute (item, "role", "s", "check");

      g_signal_connect_object (config,
                               "notify",
                               G_CALLBACK (config_notify_cb),
                               item,
                               0);
      config_notify_cb (config, NULL, item);
      g_menu_insert_item (self->config_menu, position + i, item);
    }
}

static void
ide_config_manager_finalize (GObject *object)
{
  IdeConfigManager *self = (IdeConfigManager *)object;

  g_clear_object (&self->menu);
  g_clear_object (&self->config_menu);
  g_clear_object (&self->current);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->configs, g_array_unref);

  G_OBJECT_CLASS (ide_config_manager_parent_class)->finalize (object);
}

static void
ide_config_manager_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeConfigManager *self = IDE_CONFIG_MANAGER (object);

  switch (prop_id)
    {
    case PROP_CURRENT:
      g_value_set_object (value, ide_config_manager_get_current (self));
      break;

    case PROP_CURRENT_DISPLAY_NAME:
      g_value_set_string (value, ide_config_manager_get_display_name (self));
      break;

    case PROP_READY:
      g_value_set_boolean (value, ide_config_manager_get_ready (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_config_manager_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeConfigManager *self = IDE_CONFIG_MANAGER (object);

  switch (prop_id)
    {
    case PROP_CURRENT:
      ide_config_manager_set_current (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_config_manager_class_init (IdeConfigManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = ide_config_manager_finalize;
  object_class->get_property = ide_config_manager_get_property;
  object_class->set_property = ide_config_manager_set_property;

  i_object_class->destroy = ide_config_manager_destroy;

  properties [PROP_CURRENT] =
    g_param_spec_object ("current",
                         "Current",
                         "The current configuration for the context",
                         IDE_TYPE_CONFIG,
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
   * IdeConfigManager::invalidate:
   * @self: an #IdeConfigManager
   *
   * This signal is emitted any time a new configuration is selected or the
   * currently selected configurations state changes.
   */
  signals [INVALIDATE] =
    g_signal_new ("invalidate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [INVALIDATE],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);
}

static void
ide_config_manager_init (IdeConfigManager *self)
{
  self->cancellable = g_cancellable_new ();
  self->configs = g_array_new (FALSE, FALSE, sizeof (ConfigInfo));
  g_array_set_clear_func (self->configs, config_info_clear);

  self->menu = g_menu_new ();
  self->config_menu = g_menu_new ();
  g_menu_append_submenu (self->menu,
                         _("Active Configuration"),
                         G_MENU_MODEL (self->config_menu));

  g_signal_connect (self,
                    "items-changed",
                    G_CALLBACK (items_changed_cb),
                    NULL);
}

static GType
ide_config_manager_get_item_type (GListModel *model)
{
  return IDE_TYPE_CONFIG;
}

static guint
ide_config_manager_get_n_items (GListModel *model)
{
  IdeConfigManager *self = (IdeConfigManager *)model;

  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (self->configs != NULL);

  return self->configs->len;
}

static gpointer
ide_config_manager_get_item (GListModel *model,
                             guint       position)
{
  IdeConfigManager *self = (IdeConfigManager *)model;
  const ConfigInfo *info;

  g_return_val_if_fail (IDE_IS_CONFIG_MANAGER (self), NULL);
  g_return_val_if_fail (position < self->configs->len, NULL);

  info = &g_array_index (self->configs, ConfigInfo, position);

  return g_object_ref (info->config);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_config_manager_get_item_type;
  iface->get_n_items = ide_config_manager_get_n_items;
  iface->get_item = ide_config_manager_get_item;
}

static gboolean
ide_config_manager_do_save (gpointer data)
{
  IdeConfigManager *self = data;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIG_MANAGER (self));

  self->queued_save_source = 0;

  if (self->save_needs_invalidate)
    {
      self->save_needs_invalidate = FALSE;
      g_signal_emit (self, signals [INVALIDATE], 0);
    }

  ide_config_manager_save_async (self, NULL, NULL, NULL);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_config_manager_changed (IdeConfigManager *self,
                            IdeConfig        *config)
{
  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (IDE_IS_CONFIG (config));

  if (self->queued_save_source != 0)
    return;

  /* We only care if the changed causes the config to become dirty
   * and therefore needs a writeback.
   */
  if (!ide_config_get_dirty (config))
    return;

  ide_object_message (self,
                      _("Configuration %s changed"),
                      ide_config_get_display_name (config));

  self->save_needs_invalidate |= config == self->current;

  self->queued_save_source =
    g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                WRITEBACK_DELAY_SEC,
                                ide_config_manager_do_save,
                                g_object_ref (self),
                                g_object_unref);
}

static void
ide_config_manager_config_added (IdeConfigManager  *self,
                                 IdeConfig         *config,
                                 IdeConfigProvider *provider)
{
  ConfigInfo info = {0};

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (IDE_IS_CONFIG (config));
  g_assert (IDE_IS_CONFIG_PROVIDER (provider));

  g_signal_connect_object (config,
                           "changed",
                           G_CALLBACK (ide_config_manager_changed),
                           self,
                           G_CONNECT_SWAPPED);

  info.provider = g_object_ref (provider);
  info.config = g_object_ref (config);
  g_array_append_val (self->configs, info);

  g_list_model_items_changed (G_LIST_MODEL (self), self->configs->len - 1, 0, 1);

  if (self->current == NULL)
    ide_config_manager_set_current (self, config);

  _ide_config_attach (config);

  IDE_EXIT;
}

static void
ide_config_manager_config_removed (IdeConfigManager  *self,
                                   IdeConfig         *config,
                                   IdeConfigProvider *provider)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (IDE_IS_CONFIG (config));
  g_assert (IDE_IS_CONFIG_PROVIDER (provider));

  for (guint i = 0; i < self->configs->len; i++)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, i);

      if (info->provider == provider && info->config == config)
        {
          g_signal_handlers_disconnect_by_func (config,
                                                G_CALLBACK (ide_config_manager_changed),
                                                self);
          g_array_remove_index (self->configs, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          IDE_EXIT;
        }
    }

  IDE_EXIT;
}

static void
ide_config_manager_provider_load_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeConfigProvider *provider = (IdeConfigProvider *)object;
  IdeContext *context;
  g_autoptr(IdeConfigManager) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIG_PROVIDER (provider));
  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (IDE_IS_TASK (result));

  context = ide_object_get_context (IDE_OBJECT (self));

  if (!ide_config_provider_load_finish (provider, result, &error))
    ide_context_warning (context,
                         "Failed to initialize config provider: %s: %s",
                         G_OBJECT_TYPE_NAME (provider), error->message);

  IDE_EXIT;
}

static void
provider_connect (IdeConfigManager  *self,
                  IdeConfigProvider *provider)
{
  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (IDE_IS_CONFIG_PROVIDER (provider));

  g_signal_connect_object (provider,
                           "added",
                           G_CALLBACK (ide_config_manager_config_added),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (provider,
                           "removed",
                           G_CALLBACK (ide_config_manager_config_removed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
provider_disconnect (IdeConfigManager  *self,
                     IdeConfigProvider *provider)
{
  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (IDE_IS_CONFIG_PROVIDER (provider));

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_config_manager_config_added),
                                        self);
  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_config_manager_config_removed),
                                        self);
}

static void
ide_config_manager_provider_added (PeasExtensionSet *set,
                                   PeasPluginInfo   *plugin_info,
                                   GObject    *exten,
                                   gpointer          user_data)
{
  IdeConfigManager *self = user_data;
  IdeConfigProvider *provider = (IdeConfigProvider *)exten;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIG_PROVIDER (provider));

  provider_connect (self, provider);

  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (provider));

  ide_config_provider_load_async (provider,
                                         self->cancellable,
                                         ide_config_manager_provider_load_cb,
                                         g_object_ref (self));
}

static void
ide_config_manager_provider_removed (PeasExtensionSet *set,
                                     PeasPluginInfo   *plugin_info,
                                     GObject    *exten,
                                     gpointer          user_data)
{
  IdeConfigManager *self = user_data;
  IdeConfigProvider *provider = (IdeConfigProvider *)exten;
  g_autoptr(IdeConfigProvider) hold = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIG_PROVIDER (provider));

  hold = g_object_ref (provider);

  ide_config_provider_unload (provider);

  provider_disconnect (self, provider);

  for (guint i = self->configs->len; i > 0; i--)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, i - 1);

      if (info->provider == provider)
        {
          g_warning ("%s failed to remove configuration \"%s\"",
                     G_OBJECT_TYPE_NAME (provider),
                     ide_config_get_id (info->config));
          g_array_remove_index (self->configs, i);
        }
    }

  ide_object_destroy (IDE_OBJECT (provider));
}

static void
notify_providers_loaded (IdeConfigManager *self,
                         GParamSpec       *pspec,
                         IdeTask          *task)
{
  g_autoptr(GVariant) user_value = NULL;

  g_assert (IDE_IS_CONFIG_MANAGER (self));
  g_assert (IDE_IS_TASK (task));

  if (self->project_settings == NULL)
    return;

  /*
   * At this point, all of our configuration providers have returned from
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
      const char *config_id = g_variant_get_string (user_value, NULL);
      IdeConfig *config;

      if ((config = ide_config_manager_get_config (self, config_id)))
        {
          if (config != self->current)
            ide_config_manager_set_current (self, config);
        }
      else
        {
          /* We failed to locate the user's config-id that we last used.
           * Notify the user so they have some sort of indication of
           * build pipeline failure.
           */
          ide_object_message (IDE_OBJECT (self),
                              /* translators: %s will be replaced with the build configuration identifier */
                              _("Failed to locate build configuration “%s”. It may be invalid or incorrectly formatted."),
                              config_id);
        }
    }

  self->propagate_to_settings = TRUE;
}

static void
ide_config_manager_init_load_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeConfigProvider *provider = (IdeConfigProvider *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  GPtrArray *providers;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIG_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_config_provider_load_finish (provider, result, &error))
    {
      g_assert (error != NULL);
      g_warning ("Failed to initialize config provider: %s: %s",
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
ide_config_manager_init_async (GAsyncInitable      *initable,
                               gint                 priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeConfigManager *self = (IdeConfigManager *)initable;
  g_autoptr(GPtrArray) providers = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *settings_path = NULL;
  g_autofree char *project_id = NULL;
  IdeContext *context;

  g_assert (G_IS_ASYNC_INITABLE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_config_manager_init_async);
  ide_task_set_priority (task, priority);

  g_signal_connect_swapped (task,
                            "notify::completed",
                            G_CALLBACK (notify_providers_loaded),
                            self);

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  /* Use GSettings directly because we don't want to inherit a value for
   * "config-id" from the app-wide settings here.
   */
  project_id = ide_context_dup_project_id (context);
  settings_path = ide_settings_resolve_schema_path ("org.gnome.builder.project", project_id, NULL);
  self->project_settings = g_settings_new_with_path ("org.gnome.builder.project", settings_path);

  self->providers = peas_extension_set_new (peas_engine_get_default (),
                                            IDE_TYPE_CONFIG_PROVIDER,
                                            NULL);

  g_signal_connect (self->providers,
                    "extension-added",
                    G_CALLBACK (ide_config_manager_provider_added),
                    self);

  g_signal_connect (self->providers,
                    "extension-removed",
                    G_CALLBACK (ide_config_manager_provider_removed),
                    self);

  /* We don't call ide_config_manager_provider_added() here for each
   * of our providers because we want to be in control of the async lifetime
   * and delay our init_async() completion until loaders have finished
   */

  providers = g_ptr_array_new_with_free_func (g_object_unref);
  peas_extension_set_foreach (self->providers,
                              ide_config_manager_collect_providers,
                              providers);
  ide_task_set_task_data (task, g_ptr_array_ref (providers), g_ptr_array_unref);

  for (guint i = 0; i < providers->len; i++)
    {
      IdeConfigProvider *provider = g_ptr_array_index (providers, i);

      g_assert (IDE_IS_CONFIG_PROVIDER (provider));

      provider_connect (self, provider);

      ide_object_append (IDE_OBJECT (self), IDE_OBJECT (provider));

      ide_config_provider_load_async (provider,
                                             cancellable,
                                             ide_config_manager_init_load_cb,
                                             g_object_ref (task));
    }

  if (providers->len == 0)
    ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_config_manager_init_finish (GAsyncInitable  *initable,
                                GAsyncResult    *result,
                                GError         **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIG_MANAGER (initable));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_config_manager_init_async;
  iface->init_finish = ide_config_manager_init_finish;
}

void
ide_config_manager_set_current (IdeConfigManager *self,
                                IdeConfig        *current)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONFIG_MANAGER (self));
  g_return_if_fail (!current || IDE_IS_CONFIG (current));

  if (self->current != current)
    {
      const char *id = "";

      if (self->current != NULL)
        {
          g_signal_handlers_disconnect_by_func (self->current,
                                                G_CALLBACK (ide_config_manager_notify_display_name),
                                                self);
          g_signal_handlers_disconnect_by_func (self->current,
                                                G_CALLBACK (ide_config_manager_notify_ready),
                                                self);
          g_clear_object (&self->current);
        }

      if (current != NULL)
        {
          self->current = g_object_ref (current);

          g_signal_connect_object (current,
                                   "notify::display-name",
                                   G_CALLBACK (ide_config_manager_notify_display_name),
                                   self,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (current,
                                   "notify::ready",
                                   G_CALLBACK (ide_config_manager_notify_ready),
                                   self,
                                   G_CONNECT_SWAPPED);

          if (self->propagate_to_settings && self->project_settings != NULL)
            {
              g_autofree gchar *new_id = g_strdup (ide_config_get_id (current));
              g_settings_set_string (self->project_settings, "config-id", new_id);
            }

          id = ide_config_get_id (self->current);
        }

      ide_config_manager_set_action_state (self, "current", g_variant_new_string (id));

      ide_object_message (IDE_OBJECT (self),
                          /* translators: %s is set to the identifier of the build configuration */
                          _("Active configuration set to “%s”"),
                          id);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_DISPLAY_NAME]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READY]);

      g_signal_emit (self, signals [INVALIDATE], 0);
    }
}

/**
 * ide_config_manager_ref_current:
 * @self: An #IdeConfigManager
 *
 * Gets the current configuration to use for building.
 *
 * Many systems allow you to pass a configuration in instead of relying on the
 * default configuration. This gets the default configuration that various
 * background items might use, such as tags builders which need to discover
 * settings.
 *
 * Returns: (transfer full): An #IdeConfig
 */
IdeConfig *
ide_config_manager_ref_current (IdeConfigManager *self)
{
  g_autoptr(IdeConfig) ret = NULL;

  g_return_val_if_fail (IDE_IS_CONFIG_MANAGER (self), NULL);
  g_return_val_if_fail (self->current != NULL || self->configs->len > 0, NULL);

  ide_object_lock (IDE_OBJECT (self));

  if (self->current != NULL)
    ret = g_object_ref (self->current);
  else if (self->configs->len > 0)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, 0);

      g_assert (IDE_IS_CONFIG_PROVIDER (info->provider));
      g_assert (IDE_IS_CONFIG (info->config));

      ret = g_object_ref (info->config);
    }

  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * ide_config_manager_get_current:
 * @self: An #IdeConfigManager
 *
 * Gets the current configuration to use for building.
 *
 * Many systems allow you to pass a configuration in instead of relying on the
 * default configuration. This gets the default configuration that various
 * background items might use, such as tags builders which need to discover
 * settings.
 *
 * Returns: (transfer none): An #IdeConfig
 */
IdeConfig *
ide_config_manager_get_current (IdeConfigManager *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CONFIG_MANAGER (self), NULL);
  g_return_val_if_fail (self->current != NULL || self->configs->len > 0, NULL);

  if (self->current != NULL)
    return self->current;

  if (self->configs->len > 0)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, 0);

      g_assert (IDE_IS_CONFIG_PROVIDER (info->provider));
      g_assert (IDE_IS_CONFIG (info->config));

      return info->config;
    }

  g_critical ("Failed to locate activate configuration. This should not happen.");

  return NULL;
}

void
ide_config_manager_duplicate (IdeConfigManager *self,
                              IdeConfig        *config)
{
  g_return_if_fail (IDE_IS_CONFIG_MANAGER (self));
  g_return_if_fail (IDE_IS_CONFIG (config));

  for (guint i = 0; i < self->configs->len; i++)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, i);

      g_assert (IDE_IS_CONFIG_PROVIDER (info->provider));
      g_assert (IDE_IS_CONFIG (info->config));

      if (info->config == config)
        {
          g_autoptr(IdeConfigProvider) provider = g_object_ref (info->provider);

          info = NULL; /* info becomes invalid */
          ide_config_provider_duplicate (provider, config);
          ide_config_provider_save_async (provider, NULL, NULL, NULL);
          break;
        }
    }
}

void
ide_config_manager_delete (IdeConfigManager *self,
                           IdeConfig        *config)
{
  g_autoptr(IdeConfig) hold = NULL;

  g_return_if_fail (IDE_IS_CONFIG_MANAGER (self));
  g_return_if_fail (IDE_IS_CONFIG (config));

  hold = g_object_ref (config);

  for (guint i = 0; i < self->configs->len; i++)
    {
      const ConfigInfo *info = &g_array_index (self->configs, ConfigInfo, i);
      g_autoptr(IdeConfigProvider) provider = NULL;

      g_assert (IDE_IS_CONFIG_PROVIDER (info->provider));
      g_assert (IDE_IS_CONFIG (info->config));

      provider = g_object_ref (info->provider);

      if (info->config == config)
        {
          info = NULL; /* info becomes invalid */
          ide_config_provider_delete (provider, config);
          ide_config_provider_save_async (provider, NULL, NULL, NULL);
          break;
        }
    }
}

/**
 * ide_config_manager_get_ready:
 * @self: an #IdeConfigManager
 *
 * This returns %TRUE if the current configuration is ready for usage.
 *
 * This is equivalent to checking the ready property of the current
 * configuration. It allows consumers to not need to track changes to
 * the current configuration.
 *
 * Returns: %TRUE if the current configuration is ready for usage;
 *   otherwise %FALSE.
 */
gboolean
ide_config_manager_get_ready (IdeConfigManager *self)
{
  IdeConfig *config;

  g_return_val_if_fail (IDE_IS_CONFIG_MANAGER (self), FALSE);

  if ((config = ide_config_manager_get_current (self)))
    return ide_config_get_ready (config);

  return FALSE;
}

/**
 * ide_config_manager_ref_from_context:
 * @context: an #IdeContext
 *
 * Thread-safe version of ide_config_manager_from_context().
 *
 * Returns: (transfer full): an #IdeConfigManager
 */
IdeConfigManager *
ide_config_manager_ref_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CONFIG_MANAGER);
}

/**
 * ide_config_manager_get_menu:
 * @self: a #IdeConfigManager
 *
 * Gets the menu for the config manager.
 *
 * Returns: (transfer none): a #GMenuModel
 */
GMenuModel *
ide_config_manager_get_menu (IdeConfigManager *self)
{
  g_return_val_if_fail (IDE_IS_CONFIG_MANAGER (self), NULL);

  return G_MENU_MODEL (self->menu);
}

void
ide_config_manager_invalidate (IdeConfigManager *self)
{
  IdeRuntime *runtime;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONFIG_MANAGER (self));

  if (self->current != NULL &&
      (runtime = ide_config_get_runtime (self->current)))
    ide_runtime_prepare_configuration (runtime, self->current);
}
