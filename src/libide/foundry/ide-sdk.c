/* ide-sdk.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-sdk"

#include "config.h"

#include "ide-sdk-private.h"
#include "ide-sdk-provider.h"

typedef struct
{
  IdeSdkProvider *provider;
  char *title;
  char *subtitle;
  guint can_update : 1;
  guint installed : 1;
} IdeSdkPrivate;

enum {
  PROP_0,
  PROP_CAN_UPDATE,
  PROP_INSTALLED,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeSdk, ide_sdk, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_sdk_dispose (GObject *object)
{
  IdeSdk *self = (IdeSdk *)object;
  IdeSdkPrivate *priv = ide_sdk_get_instance_private (self);

  g_clear_weak_pointer (&priv->provider);
  g_clear_pointer (&priv->subtitle, g_free);
  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (ide_sdk_parent_class)->dispose (object);
}

static void
ide_sdk_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  IdeSdk *self = IDE_SDK (object);

  switch (prop_id)
    {
    IDE_GET_PROPERTY_BOOLEAN (ide_sdk, can_update, CAN_UPDATE);
    IDE_GET_PROPERTY_BOOLEAN (ide_sdk, installed, INSTALLED);
    IDE_GET_PROPERTY_STRING (ide_sdk, subtitle, SUBTITLE);
    IDE_GET_PROPERTY_STRING (ide_sdk, title, TITLE);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_sdk_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  IdeSdk *self = IDE_SDK (object);

  switch (prop_id)
    {
    IDE_SET_PROPERTY_BOOLEAN (ide_sdk, can_update, CAN_UPDATE);
    IDE_SET_PROPERTY_BOOLEAN (ide_sdk, installed, INSTALLED);
    IDE_SET_PROPERTY_STRING (ide_sdk, subtitle, SUBTITLE);
    IDE_SET_PROPERTY_STRING (ide_sdk, title, TITLE);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_sdk_class_init (IdeSdkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_sdk_dispose;
  object_class->get_property = ide_sdk_get_property;
  object_class->set_property = ide_sdk_set_property;

  IDE_DEFINE_BOOLEAN_PROPERTY ("can-update", FALSE, G_PARAM_READWRITE, CAN_UPDATE)
  IDE_DEFINE_BOOLEAN_PROPERTY ("installed", FALSE, G_PARAM_READWRITE, INSTALLED)
  IDE_DEFINE_STRING_PROPERTY ("subtitle", NULL, G_PARAM_READWRITE, SUBTITLE)
  IDE_DEFINE_STRING_PROPERTY ("title", NULL, G_PARAM_READWRITE, TITLE)

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_sdk_init (IdeSdk *self)
{
}

IDE_DEFINE_BOOLEAN_GETTER_PRIVATE (ide_sdk, IdeSdk, IDE_TYPE_SDK, can_update)
IDE_DEFINE_BOOLEAN_GETTER_PRIVATE (ide_sdk, IdeSdk, IDE_TYPE_SDK, installed)
IDE_DEFINE_STRING_GETTER_PRIVATE (ide_sdk, IdeSdk, IDE_TYPE_SDK, subtitle)
IDE_DEFINE_STRING_GETTER_PRIVATE (ide_sdk, IdeSdk, IDE_TYPE_SDK, title)

IDE_DEFINE_BOOLEAN_SETTER_PRIVATE (ide_sdk, IdeSdk, IDE_TYPE_SDK, can_update, CAN_UPDATE)
IDE_DEFINE_BOOLEAN_SETTER_PRIVATE (ide_sdk, IdeSdk, IDE_TYPE_SDK, installed, INSTALLED)
IDE_DEFINE_STRING_SETTER_PRIVATE (ide_sdk, IdeSdk, IDE_TYPE_SDK, subtitle, SUBTITLE)
IDE_DEFINE_STRING_SETTER_PRIVATE (ide_sdk, IdeSdk, IDE_TYPE_SDK, title, TITLE)

void
_ide_sdk_set_provider (IdeSdk         *self,
                       IdeSdkProvider *provider)
{
  IdeSdkPrivate *priv = ide_sdk_get_instance_private (self);

  g_return_if_fail (IDE_IS_SDK (self));
  g_return_if_fail (!provider || IDE_IS_SDK_PROVIDER (provider));

  g_set_weak_pointer (&priv->provider, provider);
}

/**
 * ide_sdk_get_provider:
 * @self: a #IdeSdk
 *
 * Gets the provider of the SDK.
 *
 * Returns: (transfer none): an #IdeSdkProvider
 */
IdeSdkProvider *
ide_sdk_get_provider (IdeSdk *self)
{
  IdeSdkPrivate *priv = ide_sdk_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SDK (self), NULL);

  return priv->provider;
}
