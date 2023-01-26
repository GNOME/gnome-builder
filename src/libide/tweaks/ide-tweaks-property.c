/* ide-tweaks-property.c
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

#define G_LOG_DOMAIN "ide-tweaks-property"

#include "config.h"

#include "ide-tweaks-property.h"

struct _IdeTweaksProperty
{
  IdeTweaksBinding parent_instance;
  GWeakRef instance;
  GParamSpec *pspec;
  const char *name;
  gulong notify_handler;
};

enum {
  PROP_0,
  PROP_OBJECT,
  PROP_NAME,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksProperty, ide_tweaks_property, IDE_TYPE_TWEAKS_BINDING)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_property_object_notify_cb (IdeTweaksProperty *self,
                                      GParamSpec        *pspec,
                                      GObject           *instance)
{
  g_assert (IDE_IS_TWEAKS_PROPERTY (self));
  g_assert (G_IS_OBJECT (instance));

  ide_tweaks_binding_changed (IDE_TWEAKS_BINDING (self));
}

static GObject *
ide_tweaks_property_acquire (IdeTweaksProperty *self)
{
  g_autoptr(GObject) instance = NULL;

  g_assert (IDE_IS_TWEAKS_PROPERTY (self));

  if (self->name == NULL)
    return NULL;

  if ((instance = g_weak_ref_get (&self->instance)))
    {
      if (self->notify_handler == 0)
        {
          g_autofree char *signal_name = g_strdup_printf ("notify::%s", self->name);

          self->pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (instance), self->name);

          if (self->pspec == NULL)
            g_critical ("Object %s has no property named %s",
                        G_OBJECT_TYPE_NAME (instance), self->name);
          else
            self->notify_handler =
              g_signal_connect_object (instance,
                                       signal_name,
                                       G_CALLBACK (ide_tweaks_property_object_notify_cb),
                                       self,
                                       G_CONNECT_SWAPPED);
        }
    }

  return g_steal_pointer (&instance);
}

static void
ide_tweaks_property_release (IdeTweaksProperty *self)
{
  g_autoptr(GObject) instance = NULL;

  g_assert (IDE_IS_TWEAKS_PROPERTY (self));

  if ((instance = g_weak_ref_get (&self->instance)))
    g_clear_signal_handler (&self->notify_handler, instance);

  self->pspec = NULL;
  self->notify_handler = 0;
  g_weak_ref_set (&self->instance, NULL);
}

static gboolean
ide_tweaks_property_get_value (IdeTweaksBinding *binding,
                               GValue           *value)
{
  IdeTweaksProperty *self = (IdeTweaksProperty *)binding;
  g_autoptr(GObject) instance = NULL;

  g_return_val_if_fail (IDE_IS_TWEAKS_PROPERTY (self), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);
  g_return_val_if_fail (self->name != NULL, FALSE);

  if ((instance = ide_tweaks_property_acquire (self)))
    g_object_get_property (instance, self->name, value);

  return instance != NULL;
}

static void
ide_tweaks_property_set_value (IdeTweaksBinding *binding,
                               const GValue     *value)
{
  IdeTweaksProperty *self = (IdeTweaksProperty *)binding;
  g_autoptr(GObject) instance = NULL;

  g_return_if_fail (IDE_IS_TWEAKS_PROPERTY (self));
  g_return_if_fail (G_IS_VALUE (value));
  g_return_if_fail (self->name != NULL);

  if ((instance = ide_tweaks_property_acquire (self)))
    g_object_set_property (instance, self->name, value);
}

static gboolean
ide_tweaks_property_set_object_internal (IdeTweaksProperty *self,
                                         GObject           *object)
{
  g_autoptr(GObject) previous = NULL;

  g_assert (IDE_IS_TWEAKS_PROPERTY (self));
  g_assert (!object || G_IS_OBJECT (object));

  previous = g_weak_ref_get (&self->instance);
  if (previous == object)
    return FALSE;

  ide_tweaks_property_release (self);

  g_weak_ref_set (&self->instance, object);

  return TRUE;
}

static GType
ide_tweaks_property_get_expected_type (IdeTweaksBinding *binding)
{
  IdeTweaksProperty *self = (IdeTweaksProperty *)binding;
  g_autoptr(GObject) instance = NULL;

  g_assert (IDE_IS_TWEAKS_PROPERTY (self));

  if ((instance = ide_tweaks_property_acquire (self)))
    return self->pspec->value_type;

  return G_TYPE_INVALID;
}

static GtkAdjustment *
ide_tweaks_property_create_adjustment (IdeTweaksBinding *binding)
{
  IdeTweaksProperty *self = (IdeTweaksProperty *)binding;
  double lower = .0;
  double upper = .0;
  double page_increment = 10.;
  double step_increment = 1.;

  g_assert (IDE_IS_TWEAKS_PROPERTY (self));

  if (!(ide_tweaks_property_acquire (self)))
    return NULL;

  g_assert (self->pspec != NULL);

  if (G_IS_PARAM_SPEC_DOUBLE (self->pspec))
    {
      GParamSpecDouble *pspec = G_PARAM_SPEC_DOUBLE (self->pspec);

      lower = pspec->minimum;
      upper = pspec->maximum;
    }
  else if (G_IS_PARAM_SPEC_FLOAT (self->pspec))
    {
      GParamSpecFloat *pspec = G_PARAM_SPEC_FLOAT (self->pspec);

      lower = pspec->minimum;
      upper = pspec->maximum;
    }
  else if (G_IS_PARAM_SPEC_INT (self->pspec))
    {
      GParamSpecInt *pspec = G_PARAM_SPEC_INT (self->pspec);

      lower = pspec->minimum;
      upper = pspec->maximum;
    }
  else if (G_IS_PARAM_SPEC_UINT (self->pspec))
    {
      GParamSpecUInt *pspec = G_PARAM_SPEC_UINT (self->pspec);

      lower = pspec->minimum;
      upper = pspec->maximum;
    }
  else if (G_IS_PARAM_SPEC_INT64 (self->pspec))
    {
      GParamSpecInt64 *pspec = G_PARAM_SPEC_INT64 (self->pspec);

      lower = pspec->minimum;
      upper = pspec->maximum;
    }
  else if (G_IS_PARAM_SPEC_UINT64 (self->pspec))
    {
      GParamSpecUInt64 *pspec = G_PARAM_SPEC_UINT64 (self->pspec);

      lower = pspec->minimum;
      upper = pspec->maximum;
    }
  else
    {
      return NULL;
    }

  if (G_IS_PARAM_SPEC_DOUBLE (self->pspec) ||
      G_IS_PARAM_SPEC_FLOAT (self->pspec))
    {
      double distance = ABS (upper - lower);

      if (distance <= 1.)
        {
          step_increment = .05;
          page_increment = .2;
        }
      else if (distance <= 50.)
        {
          step_increment = .1;
          page_increment = 1;
        }
    }

  return gtk_adjustment_new (0, lower, upper, step_increment, page_increment, 0);
}

static void
ide_tweaks_property_dispose (GObject *object)
{
  IdeTweaksProperty *self = (IdeTweaksProperty *)object;

  ide_tweaks_property_release (self);

  self->name = NULL;

  g_assert (self->name == NULL);
  g_assert (self->pspec == NULL);
  g_assert (self->notify_handler == 0);
  g_assert (g_weak_ref_get (&self->instance) == NULL);

  G_OBJECT_CLASS (ide_tweaks_property_parent_class)->finalize (object);
}

static void
ide_tweaks_property_finalize (GObject *object)
{
  IdeTweaksProperty *self = (IdeTweaksProperty *)object;

  g_weak_ref_clear (&self->instance);

  G_OBJECT_CLASS (ide_tweaks_property_parent_class)->finalize (object);
}

static void
ide_tweaks_property_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeTweaksProperty *self = IDE_TWEAKS_PROPERTY (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, ide_tweaks_property_get_name (self));
      break;

    case PROP_OBJECT:
      g_value_take_object (value, ide_tweaks_property_dup_object (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_property_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeTweaksProperty *self = IDE_TWEAKS_PROPERTY (object);

  switch (prop_id)
    {
    case PROP_NAME:
      ide_tweaks_property_set_name (self, g_value_get_string (value));
      break;

    case PROP_OBJECT:
      ide_tweaks_property_set_object (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_property_class_init (IdeTweaksPropertyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksBindingClass *tweaks_binding_class = IDE_TWEAKS_BINDING_CLASS (klass);

  object_class->dispose = ide_tweaks_property_dispose;
  object_class->finalize = ide_tweaks_property_finalize;
  object_class->get_property = ide_tweaks_property_get_property;
  object_class->set_property = ide_tweaks_property_set_property;

  tweaks_binding_class->get_value = ide_tweaks_property_get_value;
  tweaks_binding_class->set_value = ide_tweaks_property_set_value;
  tweaks_binding_class->get_expected_type = ide_tweaks_property_get_expected_type;
  tweaks_binding_class->create_adjustment = ide_tweaks_property_create_adjustment;

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_OBJECT] =
    g_param_spec_object ("object", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_property_init (IdeTweaksProperty *self)
{
  g_weak_ref_init (&self->instance, NULL);
}

IdeTweaksProperty *
ide_tweaks_property_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_PROPERTY, NULL);
}

const char *
ide_tweaks_property_get_name (IdeTweaksProperty *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PROPERTY (self), NULL);

  return self->name;
}

void
ide_tweaks_property_set_name (IdeTweaksProperty *self,
                              const char        *name)
{
  g_return_if_fail (IDE_IS_TWEAKS_PROPERTY (self));

  name = g_intern_string (name);

  if (name != self->name)
    {
      self->name = name;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

/**
 * ide_tweaks_property_dup_object:
 * @self: a #IdeTweaksProperty
 *
 * Gets the object to tweak the property of.
 *
 * Returns: (transfer full) (nullable): a #GObject or %NULL
 */
GObject *
ide_tweaks_property_dup_object (IdeTweaksProperty *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PROPERTY (self), NULL);

  return g_weak_ref_get (&self->instance);
}

void
ide_tweaks_property_set_object (IdeTweaksProperty *self,
                                GObject           *object)
{
  g_return_if_fail (IDE_IS_TWEAKS_PROPERTY (self));
  g_return_if_fail (!object || G_IS_OBJECT (object));

  if (ide_tweaks_property_set_object_internal (self, object))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_OBJECT]);
}
