/*
 * gbp-arduino-port.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-arduino-port"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-arduino-port.h"

struct _GbpArduinoPort
{
  IdeDevice parent_instance;

  char *address;
  char *label;
  char *protocol;
  char *protocol_label;
};

G_DEFINE_FINAL_TYPE (GbpArduinoPort, gbp_arduino_port, IDE_TYPE_DEVICE)

enum
{
  PROP_0,
  PROP_ADDRESS,
  PROP_LABEL,
  PROP_PROTOCOL,
  PROP_PROTOCOL_LABEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gbp_arduino_port_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbpArduinoPort *self = GBP_ARDUINO_PORT (object);

  switch (property_id)
    {
    case PROP_ADDRESS:
      g_set_str (&self->address, g_value_get_string (value));
      break;
    case PROP_LABEL:
      g_set_str (&self->label, g_value_get_string (value));
      break;
    case PROP_PROTOCOL:
      g_set_str (&self->protocol, g_value_get_string (value));
      break;
    case PROP_PROTOCOL_LABEL:
      g_set_str (&self->protocol_label, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gbp_arduino_port_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpArduinoPort *self = GBP_ARDUINO_PORT (object);

  switch (property_id)
    {
    case PROP_ADDRESS:
      g_value_set_string (value, self->address);
      break;
    case PROP_LABEL:
      g_value_set_string (value, self->label);
      break;
    case PROP_PROTOCOL:
      g_value_set_string (value, self->protocol);
      break;
    case PROP_PROTOCOL_LABEL:
      g_value_set_string (value, self->protocol_label);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gbp_arduino_device_get_info_async (IdeDevice          *device,
                                   GCancellable       *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer            user_data)
{
  GbpArduinoPort *self = (GbpArduinoPort *) device;
  g_autoptr (IdeTask) task = NULL;
  IdeDeviceInfo *device_info = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_ARDUINO_PORT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_arduino_device_get_info_async);

  device_info = ide_device_info_new ();
  ide_device_info_set_kind (device_info, IDE_DEVICE_KIND_MICRO_CONTROLLER);

  ide_task_return_object (task, device_info);

  IDE_EXIT;
}

static IdeDeviceInfo *
gbp_arduino_device_get_info_finish (IdeDevice    *device,
                                    GAsyncResult *result,
                                    GError      **error)
{
  IdeDeviceInfo *info;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE (device));
  g_assert (IDE_IS_TASK (result));

  info = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (info);
}

static void
gbp_arduino_port_finalize (GObject *object)
{
  GbpArduinoPort *self = GBP_ARDUINO_PORT (object);

  g_clear_pointer (&self->address , g_free);
  g_clear_pointer (&self->label , g_free);
  g_clear_pointer (&self->protocol , g_free);
  g_clear_pointer (&self->protocol_label , g_free);

  G_OBJECT_CLASS (gbp_arduino_port_parent_class)->finalize (object);
}

static void
gbp_arduino_port_class_init (GbpArduinoPortClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeDeviceClass *device_class = IDE_DEVICE_CLASS (klass);

  object_class->set_property = gbp_arduino_port_set_property;
  object_class->get_property = gbp_arduino_port_get_property;
  object_class->finalize = gbp_arduino_port_finalize;

  device_class->get_info_async = gbp_arduino_device_get_info_async;
  device_class->get_info_finish = gbp_arduino_device_get_info_finish;

  properties[PROP_ADDRESS] =
      g_param_spec_string ("address", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_LABEL] =
      g_param_spec_string ("label", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_PROTOCOL] =
      g_param_spec_string ("protocol", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_PROTOCOL_LABEL] =
      g_param_spec_string ("protocol-label", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_arduino_port_init (GbpArduinoPort *self)
{
}

GbpArduinoPort *
gbp_arduino_port_new (const char *address,
                      const char *label,
                      const char *protocol,
                      const char *protocol_label)
{
  GbpArduinoPort *self;

  self = g_object_new (GBP_TYPE_ARDUINO_PORT,
                       "address", address,
                       "label", label,
                       "protocol", protocol,
                       "protocol-label", protocol_label,
                       NULL);

  ide_device_set_display_name (IDE_DEVICE (self), label);
  ide_device_set_id (IDE_DEVICE (self), address);

  return self;
}

const char *
gbp_arduino_port_get_label (GbpArduinoPort *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PORT (self), NULL);
  return self->label;
}

const char *
gbp_arduino_port_get_address (GbpArduinoPort *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PORT (self), NULL);
  return self->address;
}

const char *
gbp_arduino_port_get_protocol (GbpArduinoPort *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PORT (self), NULL);
  return self->protocol;
}

const char *
gbp_arduino_port_get_protocol_label (GbpArduinoPort *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PORT (self), NULL);
  return self->protocol_label;
}

