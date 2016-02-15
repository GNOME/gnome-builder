/* ide-device-manager.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-device.h"
#include "ide-device-manager.h"
#include "ide-device-provider.h"

#include "local/ide-local-device.h"

struct _IdeDeviceManager
{
  IdeObject  parent_instance;

  GPtrArray        *devices;
  PeasExtensionSet *providers;
};

static void list_model_init_interface (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeDeviceManager, ide_device_manager, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_init_interface))

enum {
  PROP_0,
  PROP_SETTLED,
  LAST_PROP
};

enum {
  DEVICE_ADDED,
  DEVICE_REMOVED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];
static GParamSpec *properties [LAST_PROP];

static void
get_settled (PeasExtensionSet *set,
             PeasPluginInfo   *plugin_info,
             PeasExtension    *exten,
             gpointer          user_data)
{
  gboolean *settled = user_data;

  if (!ide_device_provider_get_settled (IDE_DEVICE_PROVIDER (exten)))
    *settled = FALSE;
}

gboolean
ide_device_manager_get_settled (IdeDeviceManager *self)
{
  gboolean settled = TRUE;

  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), FALSE);

  peas_extension_set_foreach (self->providers,
                              (PeasExtensionSetForeachFunc)get_settled,
                              &settled);

  return settled;
}

static void
ide_device_manager__provider_notify_settled (IdeDeviceManager  *self,
                                             GParamSpec        *pspec,
                                             IdeDeviceProvider *provider)
{
  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SETTLED]);
}

static void
ide_device_manager_do_add_device (IdeDeviceManager *self,
                                  IdeDevice        *device)
{
  guint position;

  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (IDE_IS_DEVICE (device));

  position = self->devices->len;
  g_ptr_array_add (self->devices, g_object_ref (device));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

static void
ide_device_manager__provider_device_added (IdeDeviceManager  *self,
                                           IdeDevice         *device,
                                           IdeDeviceProvider *provider)
{
  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));
  g_return_if_fail (IDE_IS_DEVICE (device));
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));

  ide_device_manager_do_add_device (self, device);
  g_signal_emit (self, signals [DEVICE_ADDED], 0, provider, device);
}

static void
ide_device_manager__provider_device_removed (IdeDeviceManager  *self,
                                             IdeDevice         *device,
                                             IdeDeviceProvider *provider)
{
  guint i;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));
  g_return_if_fail (IDE_IS_DEVICE (device));
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));

  if (self->devices == NULL)
    return;

  for (i = 0; i < self->devices->len; i++)
    {
      IdeDevice *current = g_ptr_array_index (self->devices, i);

      if (current == device)
        {
          g_ptr_array_remove_index (self->devices, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          g_signal_emit (self, signals [DEVICE_REMOVED], 0, provider, device);
          return;
        }
    }

  g_warning (_("The device \"%s\" could not be found."),
             ide_device_get_id (device));
}

void
ide_device_manager_add_provider (IdeDeviceManager  *self,
                                 IdeDeviceProvider *provider)
{
  g_autoptr(GPtrArray) devices = NULL;
  guint i;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));

  g_signal_connect_object (provider,
                           "notify::settled",
                           G_CALLBACK (ide_device_manager__provider_notify_settled),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (provider,
                           "device-added",
                           G_CALLBACK (ide_device_manager__provider_device_added),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (provider,
                           "device-removed",
                           G_CALLBACK (ide_device_manager__provider_device_removed),
                           self,
                           G_CONNECT_SWAPPED);

  devices = ide_device_provider_get_devices (provider);

  for (i = 0; i < devices->len; i++)
    {
      IdeDevice *device;

      device = g_ptr_array_index (devices, i);
      ide_device_manager__provider_device_added (self, device, provider);
    }
}

static void
ide_device_manager_provider_added (PeasExtensionSet *set,
                                   PeasPluginInfo   *plugin_info,
                                   PeasExtension    *exten,
                                   gpointer          user_data)
{
  IdeDeviceManager *self = user_data;
  IdeDeviceProvider *provider = (IdeDeviceProvider *)exten;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEVICE_PROVIDER (provider));

  ide_device_manager_add_provider (self, provider);
}

static void
ide_device_manager_provider_removed (PeasExtensionSet *set,
                                     PeasPluginInfo   *plugin_info,
                                     PeasExtension    *exten,
                                     gpointer          user_data)
{
  IdeDeviceManager *self = user_data;
  IdeDeviceProvider *provider = (IdeDeviceProvider *)exten;
  g_autoptr(GPtrArray) devices = NULL;
  gsize i;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEVICE_PROVIDER (provider));

  devices = ide_device_provider_get_devices (provider);

  for (i = 0; i < devices->len; i++)
    {
      IdeDevice *device = g_ptr_array_index (devices, i);

      ide_device_manager__provider_device_removed (self, device, provider);
    }

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_device_manager__provider_notify_settled),
                                        self);
  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_device_manager__provider_device_added),
                                        self);
  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_device_manager__provider_device_removed),
                                        self);
}

static void
ide_device_manager_add_providers (IdeDeviceManager *self)
{
  IdeContext *context;

  g_assert (IDE_IS_DEVICE_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  self->providers = peas_extension_set_new (peas_engine_get_default (),
                                            IDE_TYPE_DEVICE_PROVIDER,
                                            "context", context,
                                            NULL);

  g_signal_connect (self->providers,
                    "extension-added",
                    G_CALLBACK (ide_device_manager_provider_added),
                    self);

  g_signal_connect (self->providers,
                    "extension-removed",
                    G_CALLBACK (ide_device_manager_provider_removed),
                    self);

  peas_extension_set_foreach (self->providers,
                              (PeasExtensionSetForeachFunc)ide_device_manager_provider_added,
                              self);
}

/**
 * ide_device_manager_get_devices:
 *
 * Retrieves all of the devices that are registered with the #IdeDeviceManager.
 *
 * Returns: (transfer container) (element-type IdeDevice*): An array of devices
 *   registered with the #IdeManager.
 */
GPtrArray *
ide_device_manager_get_devices (IdeDeviceManager *self)
{
  GPtrArray *ret;
  guint i;

  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (i = 0; i < self->devices->len; i++)
    {
      IdeDevice *device;

      device = g_ptr_array_index (self->devices, i);
      g_ptr_array_add (ret, g_object_ref (device));
    }

  return ret;
}

static void
ide_device_manager_add_local (IdeDeviceManager *self)
{
  g_autoptr(IdeDevice) device = NULL;
  IdeContext *context;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  device = g_object_new (IDE_TYPE_LOCAL_DEVICE,
                         "context", context,
                         NULL);
  ide_device_manager_do_add_device (self, device);
}

static GType
ide_device_manager_get_item_type (GListModel *list_model)
{
  return IDE_TYPE_DEVICE;
}

static guint
ide_device_manager_get_n_items (GListModel *list_model)
{
  IdeDeviceManager *self = (IdeDeviceManager *)list_model;

  g_assert (IDE_IS_DEVICE_MANAGER (self));

  return self->devices->len;
}

gpointer
ide_device_manager_get_item (GListModel *list_model,
                             guint       position)
{
  IdeDeviceManager *self = (IdeDeviceManager *)list_model;

  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (position < self->devices->len);

  return g_object_ref (g_ptr_array_index (self->devices, position));
}

static void
ide_device_manager_constructed (GObject *object)
{
  IdeDeviceManager *self = (IdeDeviceManager *)object;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));

  G_OBJECT_CLASS (ide_device_manager_parent_class)->constructed (object);

  ide_device_manager_add_local (self);
  ide_device_manager_add_providers (self);
}

static void
ide_device_manager_finalize (GObject *object)
{
  IdeDeviceManager *self = (IdeDeviceManager *)object;

  g_clear_pointer (&self->devices, g_ptr_array_unref);
  g_clear_object (&self->providers);

  G_OBJECT_CLASS (ide_device_manager_parent_class)->finalize (object);
}

static void
ide_device_manager_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeDeviceManager *self = IDE_DEVICE_MANAGER(object);

  switch (prop_id)
    {
    case PROP_SETTLED:
      g_value_set_boolean (value, ide_device_manager_get_settled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_device_manager_class_init (IdeDeviceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_device_manager_constructed;
  object_class->finalize = ide_device_manager_finalize;
  object_class->get_property = ide_device_manager_get_property;

  properties [PROP_SETTLED] =
    g_param_spec_boolean ("settled",
                          "Settled",
                          "If the device providers have settled.",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [DEVICE_ADDED] =
    g_signal_new ("device-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_DEVICE_PROVIDER,
                  IDE_TYPE_DEVICE);

  signals [DEVICE_REMOVED] =
    g_signal_new ("device-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_DEVICE_PROVIDER,
                  IDE_TYPE_DEVICE);
}

static void
list_model_init_interface (GListModelInterface *iface)
{
  iface->get_item_type = ide_device_manager_get_item_type;
  iface->get_n_items = ide_device_manager_get_n_items;
  iface->get_item = ide_device_manager_get_item;
}

static void
ide_device_manager_init (IdeDeviceManager *self)
{
  self->devices = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * ide_device_manager_get_device:
 * @device_id: The device identifier string.
 *
 * Fetches the first device that matches the device identifier @device_id.
 *
 * Returns: (transfer none): An #IdeDevice or %NULL.
 */
IdeDevice *
ide_device_manager_get_device (IdeDeviceManager *self,
                               const gchar      *device_id)
{
  gsize i;

  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), NULL);

  for (i = 0; i < self->devices->len; i++)
    {
      IdeDevice *device;
      const gchar *id;

      device = g_ptr_array_index (self->devices, i);
      id = ide_device_get_id (device);

      if (0 == g_strcmp0 (id, device_id))
        return device;
    }

  return NULL;
}
