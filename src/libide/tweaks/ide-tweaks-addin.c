/* ide-tweaks-addin.c
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

#define G_LOG_DOMAIN "ide-tweaks-addin"

#include "config.h"

#include "ide-tweaks-addin.h"

typedef struct
{
  char   *resource_path;
  GArray *callbacks;
} IdeTweaksAddinPrivate;

typedef struct
{
  const char *name;
  GCallback   callback;
} Callback;

enum {
  PROP_0,
  PROP_RESOURCE_PATH,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeTweaksAddin, ide_tweaks_addin, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_addin_real_load (IdeTweaksAddin *self,
                            IdeTweaks      *tweaks)
{
  IdeTweaksAddinPrivate *priv = ide_tweaks_addin_get_instance_private (self);
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *uri = NULL;

  g_assert (IDE_IS_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS (tweaks));

  if (priv->callbacks != NULL)
    {
      for (guint i = 0; i < priv->callbacks->len; i++)
        {
          const Callback *callback = &g_array_index (priv->callbacks, Callback, i);
          ide_tweaks_add_callback (tweaks, callback->name, callback->callback);
        }
    }

  if (priv->resource_path == NULL)
    return;

  uri = g_strdup_printf ("resource://%s", priv->resource_path);
  file = g_file_new_for_uri (uri);

  if (!ide_tweaks_load_from_file (tweaks, file, NULL, &error))
    g_warning ("%s", error->message);
}

static void
ide_tweaks_addin_dispose (GObject *object)
{
  IdeTweaksAddin *self = (IdeTweaksAddin *)object;
  IdeTweaksAddinPrivate *priv = ide_tweaks_addin_get_instance_private (self);

  g_clear_pointer (&priv->resource_path, g_free);
  g_clear_pointer (&priv->callbacks, g_array_unref);

  G_OBJECT_CLASS (ide_tweaks_addin_parent_class)->dispose (object);
}

static void
ide_tweaks_addin_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeTweaksAddin *self = IDE_TWEAKS_ADDIN (object);

  switch (prop_id)
    {
    case PROP_RESOURCE_PATH:
      g_value_set_string (value, ide_tweaks_addin_get_resource_path (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_addin_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeTweaksAddin *self = IDE_TWEAKS_ADDIN (object);

  switch (prop_id)
    {
    case PROP_RESOURCE_PATH:
      ide_tweaks_addin_set_resource_path (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_addin_class_init (IdeTweaksAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tweaks_addin_dispose;
  object_class->get_property = ide_tweaks_addin_get_property;
  object_class->set_property = ide_tweaks_addin_set_property;

  klass->load = ide_tweaks_addin_real_load;

  properties [PROP_RESOURCE_PATH] =
    g_param_spec_string ("resource-path", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_addin_init (IdeTweaksAddin *self)
{
}

const char *
ide_tweaks_addin_get_resource_path (IdeTweaksAddin *self)
{
  IdeTweaksAddinPrivate *priv = ide_tweaks_addin_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_ADDIN (self), NULL);

  return priv->resource_path;
}

void
ide_tweaks_addin_set_resource_path (IdeTweaksAddin *self,
                                    const char     *resource_path)
{
  IdeTweaksAddinPrivate *priv = ide_tweaks_addin_get_instance_private (self);

  g_return_if_fail (IDE_IS_TWEAKS_ADDIN (self));

  if (ide_set_string (&priv->resource_path, resource_path))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RESOURCE_PATH]);
}

void
ide_tweaks_addin_load (IdeTweaksAddin *self,
                       IdeTweaks      *tweaks)
{
  g_return_if_fail (IDE_IS_TWEAKS_ADDIN (self));
  g_return_if_fail (IDE_IS_TWEAKS (tweaks));

  if (IDE_TWEAKS_ADDIN_GET_CLASS (self)->load)
    IDE_TWEAKS_ADDIN_GET_CLASS (self)->load (self, tweaks);
}

void
ide_tweaks_addin_unload (IdeTweaksAddin *self,
                         IdeTweaks      *tweaks)
{
  g_return_if_fail (IDE_IS_TWEAKS_ADDIN (self));
  g_return_if_fail (IDE_IS_TWEAKS (tweaks));

  if (IDE_TWEAKS_ADDIN_GET_CLASS (self)->unload)
    IDE_TWEAKS_ADDIN_GET_CLASS (self)->unload (self, tweaks);
}

/**
 * ide_tweaks_addin_add_callback:
 * @self: a #IdeTweaksAddin
 * @name: the name of the callback within the template scope
 * @callback: (scope forever): the address of the callback
 *
 * Adds @callback to @addin so that it is registered when the
 * tweaks template, set by ide_tweaks_addin_set_resource_path(),
 * is expanded by an #IdeTweaks.
 */
void
ide_tweaks_addin_add_callback (IdeTweaksAddin *self,
                               const char     *name,
                               GCallback       callback)
{
  IdeTweaksAddinPrivate *priv = ide_tweaks_addin_get_instance_private (self);
  Callback *element;

  g_return_if_fail (IDE_IS_TWEAKS_ADDIN (self));
  g_return_if_fail (name != NULL);
  g_return_if_fail (callback != NULL);

  if (priv->callbacks == NULL)
    priv->callbacks = g_array_new (FALSE, FALSE, sizeof (Callback));

  g_array_set_size (priv->callbacks, priv->callbacks->len + 1);

  element = &g_array_index (priv->callbacks, Callback, priv->callbacks->len - 1);
  element->name = g_intern_string (name);
  element->callback = callback;
}
