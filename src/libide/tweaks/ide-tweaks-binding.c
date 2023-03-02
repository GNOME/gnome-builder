/* ide-tweaks-binding.c
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

#define G_LOG_DOMAIN "ide-tweaks-binding"

#include "config.h"

#include "ide-tweaks-binding.h"

#include "gsettings-mapping.h"

typedef struct
{
  IdeTweaksBindingTransform get_transform;
  IdeTweaksBindingTransform set_transform;
  gpointer user_data;
  GDestroyNotify notify;
} Binding;

typedef struct
{
  gpointer    instance;
  GParamSpec *pspec;
  int         inhibit;
  Binding    *binding;
} IdeTweaksBindingPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeTweaksBinding, ide_tweaks_binding, IDE_TYPE_TWEAKS_ITEM)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static gboolean
generic_transform (const GValue *from_value,
                   GValue       *to_value)
{
  if (G_VALUE_HOLDS_DOUBLE (from_value) && !G_VALUE_HOLDS_DOUBLE (to_value))
    {
      if (G_VALUE_HOLDS_INT (to_value))
        g_value_set_int (to_value, g_value_get_double (from_value));
      else if (G_VALUE_HOLDS_UINT (to_value))
        g_value_set_uint (to_value, g_value_get_double (from_value));
      else if (G_VALUE_HOLDS_INT64 (to_value))
        g_value_set_int64 (to_value, g_value_get_double (from_value));
      else if (G_VALUE_HOLDS_UINT64 (to_value))
        g_value_set_uint64 (to_value, g_value_get_double (from_value));
      else
        goto fallback;

      return TRUE;
    }

fallback:
  if (G_VALUE_TYPE (from_value) == G_VALUE_TYPE (to_value) ||
      g_value_type_transformable (G_VALUE_TYPE (from_value), G_VALUE_TYPE (to_value)))
    {
      g_value_copy (from_value, to_value);
      return TRUE;
    }

  return FALSE;
}

static void
binding_finalize (gpointer data)
{
  Binding *binding = data;

  if (binding->notify)
    binding->notify (binding->user_data);

  binding->get_transform = NULL;
  binding->set_transform = NULL;
  binding->user_data = NULL;
  binding->notify = NULL;
}

static void
binding_unref (Binding *binding)
{
  g_atomic_rc_box_release_full (binding, binding_finalize);
}

static Binding *
binding_new (IdeTweaksBindingTransform get_transform,
             IdeTweaksBindingTransform set_transform,
             gpointer                  user_data,
             GDestroyNotify            notify)
{
  Binding *binding;

  binding = g_atomic_rc_box_new0 (Binding);
  binding->get_transform = get_transform;
  binding->set_transform = set_transform;
  binding->user_data = user_data;
  binding->notify = notify;

  return binding;
}

static gboolean
binding_get (Binding      *binding,
             const GValue *from_value,
             GValue       *to_value)
{
  if (binding->get_transform)
    return binding->get_transform (from_value, to_value, binding->user_data);
  else
    return generic_transform (from_value, to_value);
}

static gboolean
binding_set (Binding      *binding,
             const GValue *from_value,
             GValue       *to_value)
{
  if (binding->set_transform)
    return binding->set_transform (from_value, to_value, binding->user_data);
  else
    return generic_transform (from_value, to_value);
}

/**
 * ide_tweaks_binding_get_expected_type:
 * @self: a #IdeTweaksBinding
 * @type: (out): a #GType
 *
 * Gets the expected type for a binding.
 *
 * This is a best effort to determine the type and may end up being
 * different based on how bindings are applied.
 *
 * Returns: %TRUE if succesful and @type is set, otherwise %FALSE and
 *   @type is set to %G_TYPE_INVALID.
 */
gboolean
ide_tweaks_binding_get_expected_type (IdeTweaksBinding *self,
                                      GType            *type)
{
  g_assert (IDE_IS_TWEAKS_BINDING (self));
  g_assert (type != NULL);

  if (IDE_TWEAKS_BINDING_GET_CLASS (self)->get_expected_type)
    *type = IDE_TWEAKS_BINDING_GET_CLASS (self)->get_expected_type (self);
  else
    *type = G_TYPE_INVALID;

  return *type != G_TYPE_INVALID;
}

static void
ide_tweaks_binding_inhibit (IdeTweaksBinding *self)
{
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);

  g_assert (IDE_IS_TWEAKS_BINDING (self));
  g_assert (priv->inhibit >= 0);

  priv->inhibit++;
}

static void
ide_tweaks_binding_uninhibit (IdeTweaksBinding *self)
{
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);

  g_assert (IDE_IS_TWEAKS_BINDING (self));
  g_assert (priv->inhibit > 0);

  priv->inhibit--;
}

static void
ide_tweaks_binding_real_changed (IdeTweaksBinding *self)
{
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);
  g_autoptr(GObject) instance = NULL;
  g_auto(GValue) value = G_VALUE_INIT;

  g_assert (IDE_IS_TWEAKS_BINDING (self));

  if (!g_set_object (&instance, priv->instance))
    return;

  g_assert (G_IS_OBJECT (instance));
  g_assert (priv->pspec != NULL);
  g_assert (priv->inhibit > 0);

  g_value_init (&value, priv->pspec->value_type);
  if (ide_tweaks_binding_get_value (self, &value))
    {
      /* Some objects don't properly check for matching strings
       * so do it up front to avoid spurious changes. This fixes
       * an issue with libadwaita AdwEntryRow resetting the insert
       * position when changing the text with matching text.
       */
      if (G_VALUE_HOLDS_STRING (&value))
        {
          g_auto(GValue) dest = G_VALUE_INIT;

          g_value_init (&dest, G_TYPE_STRING);
          g_object_get_property (instance, priv->pspec->name, &dest);

          if (g_strcmp0 (g_value_get_string (&value),
                         g_value_get_string (&dest)) == 0)
            return;
        }

      g_object_set_property (instance, priv->pspec->name, &value);
    }
}

static void
ide_tweaks_binding_dispose (GObject *object)
{
  IdeTweaksBinding *self = (IdeTweaksBinding *)object;
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);

  priv->pspec = NULL;

  g_clear_weak_pointer (&priv->instance);

  g_clear_pointer (&priv->binding, binding_unref);

  G_OBJECT_CLASS (ide_tweaks_binding_parent_class)->dispose (object);
}

static void
ide_tweaks_binding_class_init (IdeTweaksBindingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tweaks_binding_dispose;

  klass->changed = ide_tweaks_binding_real_changed;

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTweaksBindingClass, changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
ide_tweaks_binding_init (IdeTweaksBinding *self)
{
}

void
ide_tweaks_binding_changed (IdeTweaksBinding *self)
{
  g_return_if_fail (IDE_IS_TWEAKS_BINDING (self));

  ide_tweaks_binding_inhibit (self);
  g_signal_emit (self, signals [CHANGED], 0);
  ide_tweaks_binding_uninhibit (self);
}

gboolean
ide_tweaks_binding_get_value (IdeTweaksBinding *self,
                              GValue           *value)
{
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);
  g_auto(GValue) from_value = G_VALUE_INIT;

  g_return_val_if_fail (IDE_IS_TWEAKS_BINDING (self), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (G_VALUE_TYPE (value) != G_TYPE_INVALID, FALSE);

  if (priv->binding == NULL || priv->pspec == NULL)
    return IDE_TWEAKS_BINDING_GET_CLASS (self)->get_value (self, value);

  /* TODO: You could optimize an extra GValue copy out here */
  g_value_init (&from_value, priv->pspec->value_type);
  if (IDE_TWEAKS_BINDING_GET_CLASS (self)->get_value (self, &from_value))
    return binding_get (priv->binding, &from_value, value);

  return FALSE;
}

void
ide_tweaks_binding_set_value (IdeTweaksBinding *self,
                              const GValue     *value)
{
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);
  g_auto(GValue) to_value = G_VALUE_INIT;
  GType type;

  g_return_if_fail (IDE_IS_TWEAKS_BINDING (self));
  g_return_if_fail (value != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  if (priv->binding == NULL || !ide_tweaks_binding_get_expected_type (self, &type))
    {
      IDE_TWEAKS_BINDING_GET_CLASS (self)->set_value (self, value);
      return;
    }

  /* TODO: You could optimize an extra GValue copy out here */
  g_value_init (&to_value, type);
  if (binding_set (priv->binding, value, &to_value))
    IDE_TWEAKS_BINDING_GET_CLASS (self)->set_value (self, &to_value);
}

static void
ide_tweaks_binding_instance_notify_cb (IdeTweaksBinding *self,
                                       GParamSpec       *pspec,
                                       GObject          *instance)
{
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);
  g_auto(GValue) value = G_VALUE_INIT;

  g_assert (IDE_IS_TWEAKS_BINDING (self));
  g_assert (pspec != NULL);
  g_assert (G_IS_OBJECT (instance));

  if (priv->inhibit > 0)
    return;

  g_value_init (&value, pspec->value_type);
  g_object_get_property (instance, pspec->name, &value);
  ide_tweaks_binding_set_value (self, &value);
}

void
ide_tweaks_binding_unbind (IdeTweaksBinding *self)
{
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);
  g_autoptr(GObject) instance = NULL;

  g_return_if_fail (IDE_IS_TWEAKS_BINDING (self));

  g_clear_pointer (&priv->binding, binding_unref);

  if (g_set_object (&instance, priv->instance))
    {
      g_clear_weak_pointer (&priv->instance);
      priv->pspec = NULL;

      g_signal_handlers_disconnect_by_func (instance,
                                            G_CALLBACK (ide_tweaks_binding_instance_notify_cb),
                                            self);
    }
}

static gboolean dummy_cb (gpointer data) { return FALSE; };

/**
 * ide_tweaks_binding_bind_with_transform:
 * @self: a #IdeTweaksBinding
 * @instance: a #GObject
 * @property_name: a property of @instance
 * @get_transform: (nullable) (scope async): an #IdeTweaksBindingTransform or %NULL
 * @set_transform: (nullable) (scope async): an #IdeTweaksBindingTransform or %NULL
 * @user_data: closure data for @get_transform and @set_transform
 * @notify: closure notify for @user_data
 *
 * Binds the value with an optional transform.
 */
void
ide_tweaks_binding_bind_with_transform (IdeTweaksBinding          *self,
                                        gpointer                   instance,
                                        const char                *property_name,
                                        IdeTweaksBindingTransform  get_transform,
                                        IdeTweaksBindingTransform  set_transform,
                                        gpointer                   user_data,
                                        GDestroyNotify             notify)
{
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);
  g_autofree char *signal_name = NULL;

  g_return_if_fail (IDE_IS_TWEAKS_BINDING (self));
  g_return_if_fail (G_IS_OBJECT (instance));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (priv->inhibit == 0);

  ide_tweaks_binding_unbind (self);

  if (!(priv->pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (instance), property_name)))
    {
      g_critical ("Object of type %s does not have a property named %s",
                  G_OBJECT_TYPE_NAME (instance), property_name);
      g_idle_add_full (G_PRIORITY_LOW, dummy_cb, user_data, notify);
      return;
    }

  g_set_weak_pointer (&priv->instance, instance);
  priv->binding = binding_new (get_transform, set_transform, user_data, notify);

  /* Get notifications on property changes */
  signal_name = g_strdup_printf ("notify::%s", property_name);
  g_signal_connect_object (instance,
                           signal_name,
                           G_CALLBACK (ide_tweaks_binding_instance_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Copy state to the widget */
  ide_tweaks_binding_changed (self);
}

void
ide_tweaks_binding_bind (IdeTweaksBinding *self,
                         gpointer          instance,
                         const char       *property_name)
{
  ide_tweaks_binding_bind_with_transform (self, instance, property_name, NULL, NULL, NULL, NULL);
}

/**
 * ide_tweaks_binding_dup_string:
 * @self: a #IdeTweaksBinding
 *
 * Gets the current value as a newly allocated string.
 *
 * Returns: (transfer full) (nullable): a string or %NULL
 */
char *
ide_tweaks_binding_dup_string (IdeTweaksBinding *self)
{
  g_auto(GValue) value = G_VALUE_INIT;

  g_return_val_if_fail (IDE_IS_TWEAKS_BINDING (self), NULL);

  g_value_init (&value, G_TYPE_STRING);
  if (ide_tweaks_binding_get_value (self, &value))
    return g_value_dup_string (&value);

  return NULL;
}

void
ide_tweaks_binding_set_variant (IdeTweaksBinding *self,
                                GVariant         *variant)
{
  g_auto(GValue) value = G_VALUE_INIT;
  g_autoptr(GVariant) hold = NULL;
  GType type;

  g_return_if_fail (IDE_IS_TWEAKS_BINDING (self));
  g_return_if_fail (variant != NULL);

  hold = g_variant_ref_sink (variant);

  if (!ide_tweaks_binding_get_expected_type (self, &type))
    return;

  g_value_init (&value, type);
  if (!g_settings_get_mapping (&value, variant, NULL))
    return;

  ide_tweaks_binding_set_value (self, &value);
}

void
ide_tweaks_binding_set_string (IdeTweaksBinding *self,
                               const char       *string)
{
  g_auto(GValue) value = G_VALUE_INIT;

  g_return_if_fail (IDE_IS_TWEAKS_BINDING (self));

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_static_string (&value, string);
  ide_tweaks_binding_set_value (self, &value);
}

/**
 * ide_tweaks_binding_create_adjustment:
 * @self: a #IdeTweaksBinding
 *
 * Creates a new adjustment for the setting.
 *
 * Returns: (transfer full) (nullable): A #GtkAdjustment, or %NULL if
 *   an adjustment is not supported for the binding.
 */
GtkAdjustment *
ide_tweaks_binding_create_adjustment (IdeTweaksBinding *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_BINDING (self), NULL);

  if (IDE_TWEAKS_BINDING_GET_CLASS (self)->create_adjustment)
    return IDE_TWEAKS_BINDING_GET_CLASS (self)->create_adjustment (self);

  return NULL;
}

/**
 * ide_tweaks_binding_dup_strv:
 * @self: a #IdeTweaksBinding
 *
 * Gets the value as a #GStrv.
 *
 * Returns: (transfer full) (nullable) (array zero-terminated=1) (element-type utf8): A
 *   newly allocated string array, or %NULL
 */
char **
ide_tweaks_binding_dup_strv (IdeTweaksBinding *self)
{
  g_auto(GValue) value = G_VALUE_INIT;

  g_return_val_if_fail (IDE_IS_TWEAKS_BINDING (self), NULL);

  g_value_init (&value, G_TYPE_STRV);
  if (!ide_tweaks_binding_get_value (self, &value))
    return NULL;

  return g_value_dup_boxed (&value);
}

void
ide_tweaks_binding_set_strv (IdeTweaksBinding   *self,
                             const char * const *strv)
{
  g_auto(GValue) value = G_VALUE_INIT;

  g_return_if_fail (IDE_IS_TWEAKS_BINDING (self));

  g_value_init (&value, G_TYPE_STRV);
  g_value_set_static_boxed (&value, strv);
  ide_tweaks_binding_set_value (self, &value);
}
