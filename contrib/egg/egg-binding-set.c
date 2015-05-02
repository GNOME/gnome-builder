/* egg-binding-set.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "egg-binding-set.h"

/**
 * SECTION:egg-binding-set
 * @title: EggBindingSet
 * @short_description: Manage multiple #GBinding as a group.
 *
 * This should not be confused with #GtkBindingSet.
 *
 * #EggBindingSet allows you to manage a set of #GBinding that you would like attached to the
 * same source object. This is convenience so that you can manage them as a set rather than
 * reconnecting them individually.
 */

struct _EggBindingSet
{
  GObject    parent_instance;

  GObject   *source;
  GPtrArray *lazy_bindings;
};

typedef struct
{
  const gchar   *source_property;
  const gchar   *target_property;
  GObject       *target;
  GBinding      *binding;
  GBindingFlags  binding_flags;
} LazyBinding;

G_DEFINE_TYPE (EggBindingSet, egg_binding_set, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SOURCE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

EggBindingSet *
egg_binding_set_new (void)
{
  return g_object_new (EGG_TYPE_BINDING_SET, NULL);
}

static void
lazy_binding_free (gpointer data)
{
  LazyBinding *lazy_binding = data;

  if (lazy_binding != NULL)
    {
      if (lazy_binding->binding != NULL)
        {
          g_binding_unbind (lazy_binding->binding);
          lazy_binding->binding = NULL;
        }

      g_assert (lazy_binding->target == NULL);

      lazy_binding->source_property = NULL;
      lazy_binding->target_property = NULL;
    }
}

static void
egg_binding_set_connect (EggBindingSet *self,
                         LazyBinding   *lazy_binding)
{
  g_assert (EGG_IS_BINDING_SET (self));
  g_assert (self->source != NULL);
  g_assert (lazy_binding != NULL);
  g_assert (lazy_binding->binding == NULL);
  g_assert (lazy_binding->target != NULL);
  g_assert (lazy_binding->target_property != NULL);
  g_assert (lazy_binding->source_property != NULL);

  lazy_binding->binding = g_object_bind_property (self->source,
                                                  lazy_binding->source_property,
                                                  lazy_binding->target,
                                                  lazy_binding->target_property,
                                                  lazy_binding->binding_flags);
}

static void
egg_binding_set_disconnect (EggBindingSet *self,
                            LazyBinding   *lazy_binding)
{
  GBinding *binding;

  g_assert (EGG_IS_BINDING_SET (self));
  g_assert (lazy_binding != NULL);

  binding = lazy_binding->binding;

  if (binding != NULL)
    {
      lazy_binding->binding = NULL;
      g_binding_unbind (binding);
    }
}

static void
egg_binding_set__source_weak_notify (gpointer  data,
                                     GObject  *where_object_was)
{
  EggBindingSet *self = data;
  gsize i;

  g_assert (EGG_IS_BINDING_SET (self));

  self->source = NULL;

  for (i = 0; i < self->lazy_bindings->len; i++)
    {
      LazyBinding *lazy_binding;

      lazy_binding = g_ptr_array_index (self->lazy_bindings, i);
      lazy_binding->binding = NULL;
    }
}

static void
egg_binding_set__target_weak_notify (gpointer  data,
                                     GObject  *where_object_was)
{
  EggBindingSet *self = data;
  gsize i;

  g_assert (EGG_IS_BINDING_SET (self));

  for (i = 0; i < self->lazy_bindings->len; i++)
    {
      LazyBinding *lazy_binding;

      lazy_binding = g_ptr_array_index (self->lazy_bindings, i);

      if (lazy_binding->target == where_object_was)
        {
          lazy_binding->target = NULL;
          lazy_binding->binding = NULL;

          g_ptr_array_remove_index_fast (self->lazy_bindings, i);
          break;
        }
    }
}

static void
egg_binding_set_dispose (GObject *object)
{
  EggBindingSet *self = (EggBindingSet *)object;
  gsize i;

  g_assert (EGG_IS_BINDING_SET (self));

  if (self->source != NULL)
    {
      g_object_weak_unref (self->source,
                           egg_binding_set__source_weak_notify,
                           self);
      self->source = NULL;
    }

  for (i = 0; i < self->lazy_bindings->len; i++)
    {
      LazyBinding *lazy_binding;

      lazy_binding = g_ptr_array_index (self->lazy_bindings, i);

      egg_binding_set_disconnect (self, lazy_binding);

      if (lazy_binding->target != NULL)
        {
          g_object_weak_unref (lazy_binding->target,
                               egg_binding_set__target_weak_notify,
                               self);
          lazy_binding->target = NULL;
        }
    }

  if (self->lazy_bindings->len != 0)
    g_ptr_array_remove_range (self->lazy_bindings, 0, self->lazy_bindings->len);

  G_OBJECT_CLASS (egg_binding_set_parent_class)->dispose (object);
}

static void
egg_binding_set_finalize (GObject *object)
{
  EggBindingSet *self = (EggBindingSet *)object;

  g_assert (self->lazy_bindings != NULL);
  g_assert_cmpint (self->lazy_bindings->len, ==, 0);

  g_clear_pointer (&self->lazy_bindings, g_ptr_array_unref);
  g_clear_object (&self->source);

  G_OBJECT_CLASS (egg_binding_set_parent_class)->finalize (object);
}

static void
egg_binding_set_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  EggBindingSet *self = EGG_BINDING_SET (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, egg_binding_set_get_source (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_binding_set_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  EggBindingSet *self = EGG_BINDING_SET (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      egg_binding_set_set_source (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_binding_set_class_init (EggBindingSetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = egg_binding_set_dispose;
  object_class->finalize = egg_binding_set_finalize;
  object_class->get_property = egg_binding_set_get_property;
  object_class->set_property = egg_binding_set_set_property;

  gParamSpecs [PROP_SOURCE] =
    g_param_spec_object ("source",
                         _("Source"),
                         _("The source GObject."),
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE, gParamSpecs [PROP_SOURCE]);
}

static void
egg_binding_set_init (EggBindingSet *self)
{
  self->lazy_bindings = g_ptr_array_new_with_free_func (lazy_binding_free);
}

gpointer
egg_binding_set_get_source (EggBindingSet *self)
{
  g_return_val_if_fail (EGG_IS_BINDING_SET (self), NULL);

  return self->source;
}

void
egg_binding_set_set_source (EggBindingSet *self,
                            gpointer       source)
{
  g_return_if_fail (EGG_IS_BINDING_SET (self));
  g_return_if_fail (!source || G_IS_OBJECT (source));
  g_return_if_fail (source != (gpointer)self);

  if (source != (gpointer)self->source)
    {
      if (self->source != NULL)
        {
          gsize i;

          g_object_weak_unref (self->source,
                               egg_binding_set__source_weak_notify,
                               self);
          self->source = NULL;

          for (i = 0; i < self->lazy_bindings->len; i++)
            {
              LazyBinding *lazy_binding;

              lazy_binding = g_ptr_array_index (self->lazy_bindings, i);
              egg_binding_set_disconnect (self, lazy_binding);
            }
        }

      if (source != NULL)
        {
          gsize i;

          self->source = source;
          g_object_weak_ref (self->source,
                             egg_binding_set__source_weak_notify,
                             self);

          for (i = 0; i < self->lazy_bindings->len; i++)
            {
              LazyBinding *lazy_binding;

              lazy_binding = g_ptr_array_index (self->lazy_bindings, i);
              egg_binding_set_connect (self, lazy_binding);
            }
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SOURCE]);
    }
}

void
egg_binding_set_bind (EggBindingSet *self,
                      const gchar   *source_property,
                      gpointer       target,
                      const gchar   *target_property,
                      GBindingFlags  flags)
{
  LazyBinding *lazy_binding;

  g_return_if_fail (EGG_IS_BINDING_SET (self));
  g_return_if_fail (source_property != NULL);
  g_return_if_fail (G_IS_OBJECT (target));
  g_return_if_fail (target_property != NULL);
  g_return_if_fail (target != (gpointer)self);

  lazy_binding = g_slice_new0 (LazyBinding);
  lazy_binding->source_property = g_intern_string (source_property);
  lazy_binding->target_property = g_intern_string (target_property);
  lazy_binding->target = target;
  lazy_binding->binding_flags = flags;

  g_object_weak_ref (target,
                     egg_binding_set__target_weak_notify,
                     self);

  g_ptr_array_add (self->lazy_bindings, lazy_binding);

  if (self->source != NULL)
    egg_binding_set_connect (self, lazy_binding);
}
