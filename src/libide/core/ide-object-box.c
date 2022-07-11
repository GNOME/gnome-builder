/* ide-object-box.c
 *
 * Copyright 2018 Christian Hergert <unknown@domain.org>
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

#define G_LOG_DOMAIN "ide-object-box"

#include "config.h"

#include "ide-object-box.h"
#include "ide-macros.h"

struct _IdeObjectBox
{
  IdeObject  parent_instance;
  GObject   *object;
  guint      propagate_disposal : 1;
};

G_DEFINE_FINAL_TYPE (IdeObjectBox, ide_object_box, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_OBJECT,
  PROP_PROPAGATE_DISPOSAL,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_object_box_set_object (IdeObjectBox *self,
                           GObject      *object)
{
  g_return_if_fail (IDE_IS_OBJECT_BOX (self));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (g_object_get_data (object, "IDE_OBJECT_BOX") == NULL);

  self->object = g_object_ref (object);
  g_object_set_data (self->object, "IDE_OBJECT_BOX", self);
}

/**
 * ide_object_box_new:
 *
 * Create a new #IdeObjectBox.
 *
 * Returns: (transfer full): a newly created #IdeObjectBox
 */
IdeObjectBox *
ide_object_box_new (GObject *object)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);

  return g_object_new (IDE_TYPE_OBJECT_BOX,
                       "object", object,
                       NULL);
}

static gchar *
ide_object_box_repr (IdeObject *object)
{
  g_autoptr(GObject) obj = ide_object_box_ref_object (IDE_OBJECT_BOX (object));

  if (obj != NULL)
    return g_strdup_printf ("%s object=\"%s\"",
                            G_OBJECT_TYPE_NAME (object),
                            G_OBJECT_TYPE_NAME (obj));
  else
    return IDE_OBJECT_CLASS (ide_object_box_parent_class)->repr (object);
}

static void
ide_object_box_destroy (IdeObject *object)
{
  IdeObjectBox *self = (IdeObjectBox *)object;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_OBJECT (self));

  g_object_ref (self);

  /* Clear the backpointer before any disposal to the object, since that
   * will possibly result in the object calling back into this peer object.
   */
  if (self->object)
    {
      g_object_set_data (G_OBJECT (self->object), "IDE_OBJECT_BOX", NULL);
      if (self->propagate_disposal)
        g_object_run_dispose (G_OBJECT (self->object));
    }

  IDE_OBJECT_CLASS (ide_object_box_parent_class)->destroy (object);

  g_clear_object (&self->object);

  g_object_unref (self);
}

static void
ide_object_box_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeObjectBox *self = IDE_OBJECT_BOX (object);

  switch (prop_id)
    {
    case PROP_OBJECT:
      g_value_take_object (value, ide_object_box_ref_object (self));
      break;

    case PROP_PROPAGATE_DISPOSAL:
      g_value_set_boolean (value, self->propagate_disposal);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_object_box_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeObjectBox *self = IDE_OBJECT_BOX (object);

  switch (prop_id)
    {
    case PROP_OBJECT:
      ide_object_box_set_object (self, g_value_get_object (value));
      break;

    case PROP_PROPAGATE_DISPOSAL:
      self->propagate_disposal = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_object_box_class_init (IdeObjectBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_object_box_get_property;
  object_class->set_property = ide_object_box_set_property;

  i_object_class->destroy = ide_object_box_destroy;
  i_object_class->repr = ide_object_box_repr;

  /**
   * IdeObjectBox:object:
   *
   * The "object" property contains the object that is boxed and
   * placed onto the object graph using this box.
   */
  properties [PROP_OBJECT] =
    g_param_spec_object ("object",
                         "Object",
                         "The boxed object",
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeObjectBox:propagate-disposal:
   *
   * The "propagate-disposal" property denotes if the #IdeObject:object
   * property contents should have g_object_run_dispose() called when the
   * #IdeObjectBox is destroyed.
   *
   * This is useful when you want to force disposal of an external object
   * when @self is removed from the object tree.
   */
  properties [PROP_PROPAGATE_DISPOSAL] =
    g_param_spec_boolean ("propagate-disposal",
                          "Propagate Disposal",
                          "If the object should be disposed when the box is destroyed",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_object_box_init (IdeObjectBox *self)
{
  self->propagate_disposal = TRUE;
}

/**
 * ide_object_box_ref_object:
 * @self: an #IdeObjectBox
 *
 * Gets the boxed object.
 *
 * Returns: (transfer full) (nullable) (type GObject): a #GObject or %NULL
 */
gpointer
ide_object_box_ref_object (IdeObjectBox *self)
{
  GObject *ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_OBJECT_BOX (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  ret = self->object ? g_object_ref (self->object) : NULL;
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * ide_object_box_from_object:
 * @object: a #GObject
 *
 * Gets the #IdeObjectBox that contains @object, if any.
 *
 * This function may only be called from the main thread.
 *
 * Returns: (transfer none): an #IdeObjectBox
 */
IdeObjectBox *
ide_object_box_from_object (GObject *object)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  return g_object_get_data (G_OBJECT (object), "IDE_OBJECT_BOX");
}

/**
 * ide_object_box_contains:
 * @self: a #IdeObjectBox
 * @instance: (type GObject) (nullable): a #GObject or %NULL
 *
 * Checks if @self contains @instance.
 *
 * Returns: %TRUE if #IdeObjectBox:object matches @instance
 */
gboolean
ide_object_box_contains (IdeObjectBox *self,
                         gpointer      instance)
{
  gboolean ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_OBJECT_BOX (self), FALSE);

  ide_object_lock (IDE_OBJECT (self));
  ret = (instance == (gpointer)self->object);
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}
