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

typedef struct
{
  GWeakRef    instance;
  GParamSpec *pspec;
  int         inhibit;
} IdeTweaksBindingPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeTweaksBinding, ide_tweaks_binding, IDE_TYPE_TWEAKS_ITEM)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

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

  if (!(instance = g_weak_ref_get (&priv->instance)))
    return;

  g_assert (G_IS_OBJECT (instance));
  g_assert (priv->pspec != NULL);
  g_assert (priv->inhibit > 0);

  g_value_init (&value, priv->pspec->value_type);
  if (ide_tweaks_binding_get_value (self, &value))
    g_object_set_property (instance, priv->pspec->name, &value);
}

static void
ide_tweaks_binding_dispose (GObject *object)
{
  IdeTweaksBinding *self = (IdeTweaksBinding *)object;
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);

  g_weak_ref_set (&priv->instance, NULL);
  priv->pspec = NULL;

  G_OBJECT_CLASS (ide_tweaks_binding_parent_class)->dispose (object);
}

static void
ide_tweaks_binding_finalize (GObject *object)
{
  IdeTweaksBinding *self = (IdeTweaksBinding *)object;
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);

  g_weak_ref_clear (&priv->instance);

  G_OBJECT_CLASS (ide_tweaks_binding_parent_class)->finalize (object);
}

static void
ide_tweaks_binding_class_init (IdeTweaksBindingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tweaks_binding_dispose;
  object_class->finalize = ide_tweaks_binding_finalize;

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
  IdeTweaksBindingPrivate *priv = ide_tweaks_binding_get_instance_private (self);

  g_weak_ref_init (&priv->instance, NULL);
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
  g_return_val_if_fail (IDE_IS_TWEAKS_BINDING (self), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (G_VALUE_TYPE (value) != G_TYPE_INVALID, FALSE);

  return IDE_TWEAKS_BINDING_GET_CLASS (self)->get_value (self, value);
}

void
ide_tweaks_binding_set_value (IdeTweaksBinding *self,
                              const GValue     *value)
{
  g_return_if_fail (IDE_IS_TWEAKS_BINDING (self));
  g_return_if_fail (value != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  IDE_TWEAKS_BINDING_GET_CLASS (self)->set_value (self, value);
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

  if ((instance = g_weak_ref_get (&priv->instance)))
    {
      g_weak_ref_set (&priv->instance, NULL);
      priv->pspec = NULL;

      g_signal_handlers_disconnect_by_func (instance,
                                            G_CALLBACK (ide_tweaks_binding_instance_notify_cb),
                                            self);
    }
}

void
ide_tweaks_binding_bind (IdeTweaksBinding *self,
                         gpointer          instance,
                         const char       *property_name)
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
      return;
    }

  g_weak_ref_set (&priv->instance, instance);

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
