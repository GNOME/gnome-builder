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
  char   **resource_paths;
  GArray  *callbacks;
} IdeTweaksAddinPrivate;

typedef struct
{
  const char *name;
  GCallback   callback;
} Callback;

enum {
  PROP_0,
  PROP_RESOURCE_PATHS,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeTweaksAddin, ide_tweaks_addin, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_addin_real_load (IdeTweaksAddin *self,
                            IdeTweaks      *tweaks)
{
  IdeTweaksAddinPrivate *priv = ide_tweaks_addin_get_instance_private (self);

  g_assert (IDE_IS_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS (tweaks));

  ide_tweaks_expose_object (tweaks,
                            G_OBJECT_TYPE_NAME (self),
                            G_OBJECT (self));

  if (priv->callbacks != NULL)
    {
      for (guint i = 0; i < priv->callbacks->len; i++)
        {
          const Callback *callback = &g_array_index (priv->callbacks, Callback, i);
          ide_tweaks_add_callback (tweaks, callback->name, callback->callback);
        }
    }

  if (priv->resource_paths == NULL)
    return;

  for (guint i = 0; priv->resource_paths[i]; i++)
    {
      g_autofree char *uri = g_strdup_printf ("resource://%s", priv->resource_paths[i]);
      g_autoptr(GFile) file = g_file_new_for_uri (uri);
      g_autoptr(GError) error = NULL;

      if (!ide_tweaks_load_from_file (tweaks, file, NULL, &error))
        g_warning ("%s", error->message);
    }
}

static void
ide_tweaks_addin_dispose (GObject *object)
{
  IdeTweaksAddin *self = (IdeTweaksAddin *)object;
  IdeTweaksAddinPrivate *priv = ide_tweaks_addin_get_instance_private (self);

  g_clear_pointer (&priv->resource_paths, g_strfreev);
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
    case PROP_RESOURCE_PATHS:
      g_value_set_boxed (value, ide_tweaks_addin_get_resource_paths (self));
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
    case PROP_RESOURCE_PATHS:
      ide_tweaks_addin_set_resource_paths (self, g_value_get_boxed (value));
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

  properties [PROP_RESOURCE_PATHS] =
    g_param_spec_boxed ("resource-paths", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_addin_init (IdeTweaksAddin *self)
{
}

const char * const *
ide_tweaks_addin_get_resource_paths (IdeTweaksAddin *self)
{
  IdeTweaksAddinPrivate *priv = ide_tweaks_addin_get_instance_private (self);
  static const char *empty[] = { NULL };

  g_return_val_if_fail (IDE_IS_TWEAKS_ADDIN (self), NULL);

  if (priv->resource_paths == NULL)
    return empty;

  return (const char * const *)priv->resource_paths;
}

void
ide_tweaks_addin_set_resource_paths (IdeTweaksAddin     *self,
                                     const char * const *resource_paths)
{
  IdeTweaksAddinPrivate *priv = ide_tweaks_addin_get_instance_private (self);
  char **copy;

  g_return_if_fail (IDE_IS_TWEAKS_ADDIN (self));

  if ((const char * const *)priv->resource_paths == resource_paths)
    return;

  copy = g_strdupv ((char **)resource_paths);
  g_strfreev (priv->resource_paths);
  priv->resource_paths = copy;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RESOURCE_PATHS]);
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
