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
#include <libpeas/peas.h>

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-internal.h"
#include "ide-macros.h"

#include "buildsystem/ide-configuration-manager.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-configuration-provider.h"

struct _IdeConfigurationManager
{
  GObject           parent_instance;

  GPtrArray        *configurations;
  IdeConfiguration *current;
  PeasExtensionSet *extensions;
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

enum {
  INVALIDATE,
  N_SIGNALS
};

static GParamSpec *properties [LAST_PROP];
static guint signals [N_SIGNALS];

static void
ide_configuration_manager_add_default (IdeConfigurationManager *self)
{
  g_autoptr(IdeConfiguration) config = NULL;
  IdeContext *context;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  config = ide_configuration_new (context, "default", "local", "host");
  ide_configuration_set_display_name (config, _("Default"));
  ide_configuration_manager_add (self, config);

  if (self->configurations->len == 1)
    ide_configuration_manager_set_current (self, config);
}

static void
ide_configuration_manager_save_provider_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));
  g_assert (G_IS_TASK (task));

  if (!ide_configuration_provider_save_finish (provider, result, &error))
    g_task_return_error (task, error);
}

static void
ide_configuration_manager_save_provider (PeasExtensionSet *set,
                                         PeasPluginInfo   *plugin_info,
                                         PeasExtension    *exten,
                                         gpointer          user_data)
{
  g_autoptr(GTask) task = user_data;
  IdeConfigurationProvider *provider = (IdeConfigurationProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (provider));
  g_assert (G_IS_TASK (task));

  if (!g_task_had_error (task))
    ide_configuration_provider_save_async (provider,
                                           g_task_get_cancellable (task),
                                           ide_configuration_manager_save_provider_cb,
                                           g_object_ref (task));
}

void
ide_configuration_manager_save_async (IdeConfigurationManager *self,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  peas_extension_set_foreach (self->extensions,
                              ide_configuration_manager_save_provider,
                              g_object_ref (task));

  g_task_return_boolean (task, TRUE);

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
      IdeConfiguration *configuration = g_ptr_array_index (self->configurations, i);

      if (g_strcmp0 (id, ide_configuration_get_id (configuration)) == 0)
        return configuration;
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

  ide_configuration_provider_load (provider, self);
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
  g_autoptr(GFile) settings_file = NULL;
  g_autoptr(GError) error = NULL;
  IdeContext *context;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

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
  guint position;

  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (configuration));

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
