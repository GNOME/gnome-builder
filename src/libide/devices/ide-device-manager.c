/* ide-device-manager.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-device-manager"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-debug.h"

#include "application/ide-application.h"
#include "devices/ide-device.h"
#include "devices/ide-device-manager.h"
#include "devices/ide-device-provider.h"
#include "local/ide-local-device.h"
#include "plugins/ide-extension-util.h"

struct _IdeDeviceManager
{
  IdeObject         parent_instance;
  GPtrArray        *devices;
  PeasExtensionSet *providers;
};

static void list_model_init_interface (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeDeviceManager, ide_device_manager, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_init_interface))

static void
ide_device_manager_provider_device_added_cb (IdeDeviceManager  *self,
                                             IdeDevice         *device,
                                             IdeDeviceProvider *provider)
{
  g_autoptr(GMenuItem) menu_item = NULL;
  const gchar *display_name;
  const gchar *icon_name;
  const gchar *device_id;
  GMenu *menu;
  guint position;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (IDE_IS_DEVICE (device));
  g_assert (!provider || IDE_IS_DEVICE_PROVIDER (provider));

  device_id = ide_device_get_id (device);
  icon_name = ide_device_get_icon_name (device);
  display_name = ide_device_get_display_name (device);

  IDE_TRACE_MSG ("Discovered device %s", device_id);

  /* First add the device to the array, we'll notify observers later */
  position = self->devices->len;
  g_ptr_array_add (self->devices, g_object_ref (device));

  /* Now add a new menu item to our selection model */
  menu = dzl_application_get_menu_by_id (DZL_APPLICATION (IDE_APPLICATION_DEFAULT),
                                         "ide-device-manager-menu-section");
  menu_item = g_menu_item_new (display_name, NULL);
  g_menu_item_set_attribute (menu_item, "id", "s", device_id);
  g_menu_item_set_attribute (menu_item, "verb-icon-name", "s", icon_name ?: "computer-symbolic");
  g_menu_item_set_action_and_target (menu_item, "build-manager.device", "s", device_id);
  g_menu_append_item (menu, menu_item);

  /* Now notify about the new device */
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  IDE_EXIT;
}

static void
ide_device_manager_provider_device_removed_cb (IdeDeviceManager  *self,
                                               IdeDevice         *device,
                                               IdeDeviceProvider *provider)
{
  const gchar *device_id;
  GMenu *menu;
  guint n_items;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (IDE_IS_DEVICE (device));
  g_assert (IDE_IS_DEVICE_PROVIDER (provider));

  device_id = ide_device_get_id (device);

  menu = dzl_application_get_menu_by_id (DZL_APPLICATION (IDE_APPLICATION_DEFAULT),
                                         "ide-device-manager-menu-section");
  n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu));

  for (guint i = 0; i < n_items; i++)
    {
      g_autofree gchar *id = NULL;

      if (g_menu_model_get_item_attribute (G_MENU_MODEL (menu), i, "id", "s", &id) &&
          g_strcmp0 (id, device_id) == 0)
        {
          g_menu_remove (menu, i);
          break;
        }
    }

  for (guint i = 0; i < self->devices->len; i++)
    {
      IdeDevice *element = g_ptr_array_index (self->devices, i);

      if (element == device)
        {
          g_ptr_array_remove_index (self->devices, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          break;
        }
    }

  IDE_EXIT;
}

static void
ide_device_manager_provider_load_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeDeviceProvider *provider = (IdeDeviceProvider *)object;
  g_autoptr(IdeDeviceManager) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DEVICE_MANAGER (self));

  if (!ide_device_provider_load_finish (provider, result, &error))
    g_warning ("%s failed to load: %s",
               G_OBJECT_TYPE_NAME (provider),
               error->message);

  IDE_EXIT;
}

static void
ide_device_manager_provider_added_cb (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      PeasExtension    *exten,
                                      gpointer          user_data)
{
  IdeDeviceManager *self = user_data;
  IdeDeviceProvider *provider = (IdeDeviceProvider *)exten;
  g_autoptr(GPtrArray) devices = NULL;

  IDE_ENTRY;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEVICE_PROVIDER (provider));

  g_signal_connect_object (provider,
                           "device-added",
                           G_CALLBACK (ide_device_manager_provider_device_added_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (provider,
                           "device-removed",
                           G_CALLBACK (ide_device_manager_provider_device_removed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  devices = ide_device_provider_get_devices (provider);

  for (guint i = 0; i < devices->len; i++)
    {
      IdeDevice *device = g_ptr_array_index (devices, i);

      g_assert (IDE_IS_DEVICE (device));

      ide_device_manager_provider_device_added_cb (self, device, provider);
    }

  ide_device_provider_load_async (provider,
                                  NULL,
                                  ide_device_manager_provider_load_cb,
                                  g_object_ref (self));

  IDE_EXIT;
}

static void
ide_device_manager_provider_removed_cb (PeasExtensionSet *set,
                                        PeasPluginInfo   *plugin_info,
                                        PeasExtension    *exten,
                                        gpointer          user_data)
{
  IdeDeviceManager *self = user_data;
  IdeDeviceProvider *provider = (IdeDeviceProvider *)exten;
  g_autoptr(GPtrArray) devices = NULL;

  IDE_ENTRY;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (IDE_IS_DEVICE_MANAGER (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEVICE_PROVIDER (provider));

  devices = ide_device_provider_get_devices (provider);

  for (guint i = 0; i < devices->len; i++)
    {
      IdeDevice *removed_device = g_ptr_array_index (devices, i);

      for (guint j = 0; j < self->devices->len; j++)
        {
          IdeDevice *device = g_ptr_array_index (self->devices, j);

          if (device == removed_device)
            {
              g_ptr_array_remove_index (self->devices, j);
              g_list_model_items_changed (G_LIST_MODEL (self), j, 1, 0);
              break;
            }
        }
    }

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_device_manager_provider_device_added_cb),
                                        self);

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_device_manager_provider_device_removed_cb),
                                        self);

  IDE_EXIT;
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
                    G_CALLBACK (ide_device_manager_provider_added_cb),
                    self);

  g_signal_connect (self->providers,
                    "extension-removed",
                    G_CALLBACK (ide_device_manager_provider_removed_cb),
                    self);

  peas_extension_set_foreach (self->providers,
                              (PeasExtensionSetForeachFunc)ide_device_manager_provider_added_cb,
                              self);
}

static void
ide_device_manager_add_local (IdeDeviceManager *self)
{
  g_autoptr(IdeDevice) device = NULL;
  IdeContext *context;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  device = g_object_new (IDE_TYPE_LOCAL_DEVICE, "context", context, NULL);
  ide_device_manager_provider_device_added_cb (self, device, NULL);
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
ide_device_manager_dispose (GObject *object)
{
  IdeDeviceManager *self = (IdeDeviceManager *)object;

  if (self->devices->len > 0)
    g_ptr_array_remove_range (self->devices, 0, self->devices->len);
  g_clear_object (&self->providers);

  G_OBJECT_CLASS (ide_device_manager_parent_class)->dispose (object);
}

static void
ide_device_manager_finalize (GObject *object)
{
  IdeDeviceManager *self = (IdeDeviceManager *)object;

  g_clear_pointer (&self->devices, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_device_manager_parent_class)->finalize (object);
}

static void
ide_device_manager_class_init (IdeDeviceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_device_manager_constructed;
  object_class->dispose = ide_device_manager_dispose;
  object_class->finalize = ide_device_manager_finalize;
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
 * @self: an #IdeDeviceManager
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
  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), NULL);

  for (guint i = 0; i < self->devices->len; i++)
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
