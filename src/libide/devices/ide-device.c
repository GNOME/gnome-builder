/* ide-device.c
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

#define G_LOG_DOMAIN "ide-device"

#include <glib/gi18n.h>

#include "ide-debug.h"

#include "config/ide-configuration.h"
#include "devices/ide-device.h"

typedef struct
{
  gchar *display_name;
  gchar *icon_name;
  gchar *id;
} IdeDevicePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeDevice, ide_device, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_SYSTEM_TYPE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ide_device_get_display_name:
 *
 * This function returns the name of the device. If no name has been set, then
 * %NULL is returned.
 *
 * In some cases, this value wont be available until additional information
 * has been probed from the device.
 *
 * Returns: (nullable): A string containing the display name for the device.
 */
const gchar *
ide_device_get_display_name (IdeDevice *device)
{
  IdeDevicePrivate *priv = ide_device_get_instance_private (device);

  g_return_val_if_fail (IDE_IS_DEVICE (device), NULL);

  return priv->display_name;
}

void
ide_device_set_display_name (IdeDevice   *device,
                             const gchar *display_name)
{
  IdeDevicePrivate *priv = ide_device_get_instance_private (device);

  g_return_if_fail (IDE_IS_DEVICE (device));

  if (display_name != priv->display_name)
    {
      g_free (priv->display_name);
      priv->display_name = g_strdup (display_name);
      g_object_notify_by_pspec (G_OBJECT (device),
                                properties [PROP_DISPLAY_NAME]);
    }
}

/**
 * ide_device_get_icon_name:
 * @self: a #IdeDevice
 *
 * Gets the icon to use when displaying the device in UI elements.
 *
 * Returns: (nullable): an icon-name or %NULL
 *
 * Since: 3.28
 */
const gchar *
ide_device_get_icon_name (IdeDevice *self)
{
  IdeDevicePrivate *priv = ide_device_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEVICE (self), NULL);

  return priv->icon_name;
}

/**
 * ide_device_set_icon_name:
 * @self: a #IdeDevice
 *
 * Sets the icon-name property.
 *
 * This is the icon that is displayed with the device name in UI elements.
 *
 * Since: 3.28
 */
void
ide_device_set_icon_name (IdeDevice   *self,
                          const gchar *icon_name)
{
  IdeDevicePrivate *priv = ide_device_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEVICE (self));

  if (g_strcmp0 (icon_name, priv->icon_name) != 0)
    {
      g_free (priv->icon_name);
      priv->icon_name = g_strdup (icon_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON_NAME]);
    }
}

/**
 * ide_device_get_id:
 *
 * Retrieves the "id" property of the #IdeDevice. This is generally not a
 * user friendly name as it is often a guid.
 *
 * Returns: A unique identifier for the device.
 */
const gchar *
ide_device_get_id (IdeDevice *device)
{
  IdeDevicePrivate *priv = ide_device_get_instance_private (device);

  g_return_val_if_fail (IDE_IS_DEVICE (device), NULL);

  return priv->id;
}

void
ide_device_set_id (IdeDevice   *device,
                   const gchar *id)
{
  IdeDevicePrivate *priv = ide_device_get_instance_private (device);

  g_return_if_fail (IDE_IS_DEVICE (device));

  if (id != priv->id)
    {
      g_free (priv->id);
      priv->id = g_strdup (id);
      g_object_notify_by_pspec (G_OBJECT (device), properties [PROP_ID]);
    }
}

/**
 * ide_device_get_system_type:
 *
 * This is the description of the system we are building for. Commonly, this
 * is referred to as a "system_type". A combination of the machine architecture
 * such as x86_64, the operating system, and the libc.
 *
 * "x86_64-linux-gnu" might be one such system.
 *
 * Returns: A string containing the system type.
 */
const gchar *
ide_device_get_system_type (IdeDevice *device)
{
  IdeDeviceClass *klass;
  const gchar *ret = NULL;

  g_return_val_if_fail (IDE_IS_DEVICE (device), NULL);

  klass = IDE_DEVICE_GET_CLASS (device);

  if (klass->get_system_type)
    ret = klass->get_system_type (device);

  return ret;
}

static void
ide_device_real_get_info_async (IdeDevice           *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_device_real_get_info_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s has not implemented get_info_async()",
                           G_OBJECT_TYPE_NAME (self));
}

static IdeDeviceInfo *
ide_device_real_get_info_finish (IdeDevice     *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_device_finalize (GObject *object)
{
  IdeDevice *self = (IdeDevice *)object;
  IdeDevicePrivate *priv = ide_device_get_instance_private (self);

  g_clear_pointer (&priv->display_name, g_free);
  g_clear_pointer (&priv->id, g_free);

  G_OBJECT_CLASS (ide_device_parent_class)->finalize (object);
}

static void
ide_device_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeDevice *self = IDE_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_device_get_display_name (self));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, ide_device_get_icon_name (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_device_get_id (self));
      break;

    case PROP_SYSTEM_TYPE:
      g_value_set_string (value, ide_device_get_system_type (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_device_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeDevice *self = IDE_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      ide_device_set_display_name (self, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      ide_device_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_ID:
      ide_device_set_id (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_device_class_init (IdeDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_device_finalize;
  object_class->get_property = ide_device_get_property;
  object_class->set_property = ide_device_set_property;

  klass->get_info_async = ide_device_real_get_info_async;
  klass->get_info_finish = ide_device_real_get_info_finish;

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "The display name of the device.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDevice:icon-name:
   *
   * The "icon-name" property is the icon to display with the device in
   * various UI elements of Builder.
   *
   * Since: 3.28
   */
  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "Icon Name",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "ID",
                         "The device identifier.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SYSTEM_TYPE] =
    g_param_spec_string ("system-type",
                         "System Type",
                         "The system type for which to compile.",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_device_init (IdeDevice *self)
{
}

void
ide_device_prepare_configuration (IdeDevice        *self,
                                  IdeConfiguration *configuration)
{
  g_assert (IDE_IS_DEVICE (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  if (IDE_DEVICE_GET_CLASS (self)->prepare_configuration)
    IDE_DEVICE_GET_CLASS (self)->prepare_configuration (self, configuration);
}

GQuark
ide_device_error_quark (void)
{
  static GQuark quark = 0;

  if G_UNLIKELY (quark == 0)
    quark = g_quark_from_static_string ("ide_device_error_quark");

  return quark;
}

/**
 * ide_device_get_info_async:
 * @self: an #IdeDevice
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously requests information about the device.
 *
 * Some information may not be available until after a connection
 * has been established. This allows the device to connect before
 * fetching that information.
 *
 * Since: 3.28
 */
void
ide_device_get_info_async (IdeDevice           *self,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEVICE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEVICE_GET_CLASS (self)->get_info_async (self, cancellable, callback, user_data);

  IDE_EXIT;
}

/**
 * ide_device_get_info_finish:
 * @self: an #IdeDevice
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to load the information about a device.
 *
 * Returns: (transfer full): an #IdeDeviceInfo or %NULL and @error is set
 *
 * Since: 3.28
 */
IdeDeviceInfo *
ide_device_get_info_finish (IdeDevice     *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  IdeDeviceInfo *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_DEVICE (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = IDE_DEVICE_GET_CLASS (self)->get_info_finish (self, result, error);

  g_return_val_if_fail (!ret || IDE_IS_DEVICE_INFO (ret), NULL);

  IDE_RETURN (ret);
}
