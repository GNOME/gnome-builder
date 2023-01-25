/* ide-object.c
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-object"

#include "config.h"

#include "ide-context.h"
#include "ide-object.h"
#include "ide-macros.h"
#include "ide-marshal.h"

/**
 * SECTION:ide-object
 * @title: IdeObject
 * @short_description: Base object with support for object trees
 *
 * #IdeObject is a specialized #GObject for use in Builder. It provides a
 * hierarchy of objects using a specialized tree similar to a DOM. You can
 * insert/append/prepend objects to a parent node, and track their lifetime
 * as part of the tree.
 *
 * When an object is removed from the tree, it can automatically be destroyed
 * via the #IdeObject::destroy signal. This is useful as it may cause the
 * children of that object to be removed, recursively destroying the objects
 * descendants. This behavior is ideal when you want a large amount of objects
 * to be reclaimed once an ancestor is no longer necessary.
 *
 * #IdeObject's may also have a #GCancellable associated with them. The
 * cancellable is created on demand when ide_object_ref_cancellable() is
 * called. When the object is destroyed, the #GCancellable::cancel signal
 * is emitted. This allows automatic cleanup of asynchronous operations
 * when used properly.
 */

typedef struct
{
  GRecMutex     mutex;
  GCancellable *cancellable;
  IdeObject    *parent;
  GQueue        children;
  GList         link;
  guint         in_destruction : 1;
  guint         destroyed : 1;
} IdeObjectPrivate;

typedef struct
{
  GType      type;
  IdeObject *child;
} GetChildTyped;

typedef struct
{
  GType      type;
  GPtrArray *array;
} GetChildrenTyped;

enum {
  PROP_0,
  PROP_CANCELLABLE,
  PROP_PARENT,
  N_PROPS
};

enum {
  DESTROY,
  N_SIGNALS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeObject, ide_object, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];
static GQueue finalizer_queue = G_QUEUE_INIT;
static GMutex finalizer_mutex;
static GSource *finalizer_source;

static gboolean
ide_object_finalizer_source_check (GSource *source)
{
  return finalizer_queue.length > 0;
}

static gboolean
ide_object_finalizer_source_dispatch (GSource     *source,
                                      GSourceFunc  callback,
                                      gpointer     user_data)
{
  while (finalizer_queue.length)
    {
      g_autoptr(GObject) object = g_queue_pop_head (&finalizer_queue);
      g_object_run_dispose (object);
    }

  return G_SOURCE_CONTINUE;
}

static GSourceFuncs finalizer_source_funcs = {
  .check = ide_object_finalizer_source_check,
  .dispatch = ide_object_finalizer_source_dispatch,
};

static inline void
ide_object_private_lock (IdeObjectPrivate *priv)
{
  g_rec_mutex_lock (&priv->mutex);
}

static inline void
ide_object_private_unlock (IdeObjectPrivate *priv)
{
  g_rec_mutex_unlock (&priv->mutex);
}

static gboolean
check_disposition (IdeObject        *child,
                   IdeObject        *parent,
                   IdeObjectPrivate *sibling_priv)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (child);

  if (priv->parent != NULL)
    {
      g_critical ("Attempt to add %s to %s, but it already has a parent",
                  G_OBJECT_TYPE_NAME (child),
                  G_OBJECT_TYPE_NAME (parent));
      return FALSE;
    }

  if (sibling_priv && sibling_priv->parent != parent)
    {
      g_critical ("Attempt to add child relative to sibling of another parent");
      return FALSE;
    }

  return TRUE;
}

static gchar *
ide_object_real_repr (IdeObject *self)
{
  return g_strdup (G_OBJECT_TYPE_NAME (self));
}

static void
ide_object_real_add (IdeObject         *self,
                     IdeObject         *sibling,
                     IdeObject         *child,
                     IdeObjectLocation  location)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  IdeObjectPrivate *child_priv = ide_object_get_instance_private (child);
  IdeObjectPrivate *sibling_priv = ide_object_get_instance_private (sibling);

  g_assert (IDE_IS_OBJECT (self));
  g_assert (IDE_IS_OBJECT (child));
  g_assert (!sibling || IDE_IS_OBJECT (sibling));

  if (location == IDE_OBJECT_BEFORE_SIBLING ||
      location == IDE_OBJECT_AFTER_SIBLING)
    g_return_if_fail (IDE_IS_OBJECT (sibling));

  ide_object_private_lock (priv);
  ide_object_private_lock (child_priv);

  if (sibling)
    ide_object_private_lock (sibling_priv);

  if (!check_disposition (child, self, NULL))
    goto unlock;

  switch (location)
    {
    case IDE_OBJECT_START:
      g_queue_push_head_link (&priv->children, &child_priv->link);
      break;

    case IDE_OBJECT_END:
      g_queue_push_tail_link (&priv->children, &child_priv->link);
      break;

    case IDE_OBJECT_BEFORE_SIBLING:
      _g_queue_insert_before_link (&priv->children, &sibling_priv->link, &child_priv->link);
      break;

    case IDE_OBJECT_AFTER_SIBLING:
      _g_queue_insert_after_link (&priv->children, &sibling_priv->link, &child_priv->link);
      break;

    default:
      g_critical ("Invalid location to add object child");
      goto unlock;
    }

  child_priv->parent = self;
  g_object_ref (child);

  if (IDE_OBJECT_GET_CLASS (child)->parent_set)
    IDE_OBJECT_GET_CLASS (child)->parent_set (child, self);

unlock:
  if (sibling)
    ide_object_private_unlock (sibling_priv);
  ide_object_private_unlock (child_priv);
  ide_object_private_unlock (priv);
}

static void
ide_object_real_remove (IdeObject *self,
                        IdeObject *child)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  IdeObjectPrivate *child_priv = ide_object_get_instance_private (child);

  g_assert (IDE_IS_OBJECT (self));
  g_assert (IDE_IS_OBJECT (child));

  ide_object_private_lock (priv);
  ide_object_private_lock (child_priv);

  g_assert (child_priv->parent == self);

  if (child_priv->parent != self)
    {
      g_critical ("Attempt to remove child object from incorrect parent");
      ide_object_private_unlock (child_priv);
      ide_object_private_unlock (priv);
      return;
    }

  g_queue_unlink (&priv->children, &child_priv->link);
  child_priv->parent = NULL;

  if (IDE_OBJECT_GET_CLASS (child)->parent_set)
    IDE_OBJECT_GET_CLASS (child)->parent_set (child, NULL);

  ide_object_private_unlock (child_priv);
  ide_object_private_unlock (priv);

  g_object_unref (child);
}

static void
ide_object_real_destroy (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  IdeObject *hold = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_OBJECT (self));

  /* We already hold the instance lock, for destroy */

  g_cancellable_cancel (priv->cancellable);

  if (priv->parent != NULL)
    {
      hold = g_object_ref (self);
      ide_object_remove (priv->parent, self);
    }

  g_assert (priv->parent == NULL);
  g_assert (priv->link.prev == NULL);
  g_assert (priv->link.next == NULL);

  while (priv->children.head != NULL)
    {
      IdeObject *child = priv->children.head->data;

      ide_object_destroy (child);
    }

  g_assert (priv->children.tail == NULL);
  g_assert (priv->children.head == NULL);
  g_assert (priv->children.length == 0);

  g_assert (priv->parent == NULL);
  g_assert (priv->link.prev == NULL);
  g_assert (priv->link.next == NULL);

  priv->destroyed = TRUE;

  if (hold != NULL)
    g_object_unref (hold);
}

void
ide_object_destroy (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_return_if_fail (IDE_IS_OBJECT (self));

  g_object_ref (self);
  ide_object_private_lock (priv);

  if (!IDE_IS_MAIN_THREAD ())
    {
      g_mutex_lock (&finalizer_mutex);
      g_queue_push_tail (&finalizer_queue, g_object_ref (self));
      g_mutex_unlock (&finalizer_mutex);
      goto cleanup;
    }

  g_cancellable_cancel (priv->cancellable);
  if (!priv->in_destruction && !priv->destroyed)
    g_object_run_dispose (G_OBJECT (self));

cleanup:
  ide_object_private_unlock (priv);
  g_object_unref (self);
}

static void
ide_object_dispose (GObject *object)
{
  IdeObject *self = (IdeObject *)object;
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  if (!IDE_IS_MAIN_THREAD ())
    {
      g_mutex_lock (&finalizer_mutex);
      g_queue_push_tail (&finalizer_queue, g_object_ref (self));
      g_mutex_unlock (&finalizer_mutex);
      g_main_context_wakeup (NULL);
      return;
    }

  g_assert (IDE_IS_OBJECT (object));
  g_assert (IDE_IS_MAIN_THREAD ());

  ide_object_private_lock (priv);

  if (!priv->in_destruction)
    {
      priv->in_destruction = TRUE;
      g_signal_emit (self, signals [DESTROY], 0);
      priv->in_destruction = FALSE;
    }

  ide_object_private_unlock (priv);

  G_OBJECT_CLASS (ide_object_parent_class)->dispose (object);
}

static void
ide_object_finalize (GObject *object)
{
  IdeObject *self = (IdeObject *)object;
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_assert (priv->parent == NULL);
  g_assert (priv->children.length == 0);
  g_assert (priv->children.head == NULL);
  g_assert (priv->children.tail == NULL);
  g_assert (priv->link.prev == NULL);
  g_assert (priv->link.next == NULL);

  g_clear_object (&priv->cancellable);
  g_rec_mutex_clear (&priv->mutex);

  G_OBJECT_CLASS (ide_object_parent_class)->finalize (object);
}

static void
ide_object_constructed (GObject *object)
{
  if G_UNLIKELY (G_OBJECT_GET_CLASS (object)->dispose != ide_object_dispose)
    g_critical ("%s overrides dispose. This is not allowed, use destroy instead.",
                G_OBJECT_TYPE_NAME (object));

  G_OBJECT_CLASS (ide_object_parent_class)->constructed (object);
}

static void
ide_object_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeObject *self = IDE_OBJECT (object);

  switch (prop_id)
    {
    case PROP_PARENT:
      g_value_take_object (value, ide_object_ref_parent (self));
      break;

    case PROP_CANCELLABLE:
      g_value_take_object (value, ide_object_ref_cancellable (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_object_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeObject *self = IDE_OBJECT (object);
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CANCELLABLE:
      priv->cancellable = g_value_dup_object (value);
      break;

    case PROP_PARENT:
      {
        IdeObject *parent = g_value_get_object (value);
        if (parent != NULL)
          ide_object_append (parent, IDE_OBJECT (self));
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_object_class_init (IdeObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_object_constructed;
  object_class->dispose = ide_object_dispose;
  object_class->finalize = ide_object_finalize;
  object_class->get_property = ide_object_get_property;
  object_class->set_property = ide_object_set_property;

  klass->add = ide_object_real_add;
  klass->remove = ide_object_real_remove;
  klass->destroy = ide_object_real_destroy;
  klass->repr = ide_object_real_repr;

  /**
   * IdeObject:parent:
   *
   * The parent #IdeObject, if any.
   */
  properties [PROP_PARENT] =
    g_param_spec_object ("parent",
                         "Parent",
                         "The parent IdeObject",
                         IDE_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeObject:cancellable:
   *
   * The "cancellable" property is a #GCancellable that can be used by operations
   * that will be cancelled when the #IdeObject::destroy signal is emitted on @self.
   *
   * This is convenient when you want operations to automatically be cancelled when
   * part of teh object tree is segmented.
   */
  properties [PROP_CANCELLABLE] =
    g_param_spec_object ("cancellable",
                         "Cancellable",
                         "A GCancellable for the object to use in operations",
                         G_TYPE_CANCELLABLE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeObject::destroy:
   *
   * The "destroy" signal is emitted when the object should destroy itself
   * and cleanup any state that is no longer necessary. This happens when
   * the object has been removed from the because it was requested to be
   * destroyed, or because a parent object is being destroyed.
   *
   * If you do not want to receive the "destroy" signal, then you must
   * manually remove the object from the tree using ide_object_remove()
   * while holding a reference to the object.
   */
  signals [DESTROY] =
    g_signal_new ("destroy",
                  G_TYPE_FROM_CLASS (klass),
                  (G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS),
                  G_STRUCT_OFFSET (IdeObjectClass, destroy),
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [DESTROY],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);

  /* Setup finalizer in main thread to allow passing objects for finalization
   * across to main thread.
   */
  finalizer_source = g_source_new (&finalizer_source_funcs, sizeof (GSource));
  g_source_set_static_name (finalizer_source, "[ide-object-finalizer]");
  g_source_set_priority (finalizer_source, G_MAXINT);
  g_source_attach (finalizer_source, NULL);
}

static void
ide_object_init (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  priv->link.data = self;

  g_rec_mutex_init (&priv->mutex);
}

/**
 * ide_object_new:
 * @type: a #GType of an #IdeObject derived object
 * @parent: (nullable): an optional #IdeObject parent
 *
 * This is a convenience function for creating an #IdeObject and appending it
 * to a parent.
 *
 * This function may only be called from the main-thread, as calling from any
 * other thread would potentially risk being disposed before returning.
 *
 * Returns: (transfer full) (type IdeObject): a new #IdeObject
 */
gpointer
ide_object_new (GType      type,
                IdeObject *parent)
{
  IdeObject *ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (g_type_is_a (type, IDE_TYPE_OBJECT), NULL);
  g_return_val_if_fail (!parent || IDE_IS_OBJECT (parent), NULL);

  ret = g_object_new (type, NULL);
  if (parent != NULL)
    ide_object_append (parent, ret);

  return g_steal_pointer (&ret);
}

/**
 * ide_object_get_n_children:
 * @self: a #IdeObject
 *
 * Gets the number of children for an object.
 *
 * Returns: the number of children
 */
guint
ide_object_get_n_children (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  guint ret;

  g_return_val_if_fail (IDE_IS_OBJECT (self), 0);

  ide_object_private_lock (priv);
  ret = priv->children.length;
  ide_object_private_unlock (priv);

  return ret;
}

/**
 * ide_object_get_nth_child:
 * @self: a #IdeObject
 * @nth: position of child to fetch
 *
 * Gets the @nth child of @self.
 *
 * A full reference to the child is returned.
 *
 * Returns: (transfer full) (nullable): an #IdeObject or %NULL
 */
IdeObject *
ide_object_get_nth_child (IdeObject *self,
                          guint      nth)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  IdeObject *ret;

  g_return_val_if_fail (IDE_IS_OBJECT (self), 0);

  ide_object_private_lock (priv);
  ret = g_list_nth_data (priv->children.head, nth);
  if (ret != NULL)
    g_object_ref (ret);
  ide_object_private_unlock (priv);

  g_return_val_if_fail (!ret || IDE_IS_OBJECT (ret), NULL);

  return g_steal_pointer (&ret);
}

/**
 * ide_object_get_position:
 * @self: a #IdeObject
 *
 * Gets the position of @self within the parent node.
 *
 * Returns: the position, starting from 0
 */
guint
ide_object_get_position (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  guint ret = 0;

  g_return_val_if_fail (IDE_IS_OBJECT (self), 0);

  ide_object_private_lock (priv);

  if (priv->parent != NULL)
    {
      IdeObjectPrivate *parent_priv = ide_object_get_instance_private (priv->parent);
      ret = g_list_position (parent_priv->children.head, &priv->link);
    }

  ide_object_private_unlock (priv);

  return ret;
}

/**
 * ide_object_lock:
 * @self: a #IdeObject
 *
 * Acquires the lock for @self. This can be useful when you need to do
 * multi-threaded work with @self and want to ensure exclusivity.
 *
 * Call ide_object_unlock() to release the lock.
 *
 * The synchronization used is a #GRecMutex.
 */
void
ide_object_lock (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_return_if_fail (IDE_IS_OBJECT (self));

  ide_object_private_lock (priv);
}

/**
 * ide_object_unlock:
 * @self: a #IdeObject
 *
 * Releases a previously acuiqred lock from ide_object_lock().
 *
 * The synchronization used is a #GRecMutex.
 */
void
ide_object_unlock (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_return_if_fail (IDE_IS_OBJECT (self));

  ide_object_private_unlock (priv);
}

/**
 * ide_object_ref_cancellable:
 * @self: a #IdeObject
 *
 * Gets a #GCancellable for the object.
 *
 * Returns: (transfer none) (not nullable): a #GCancellable
 */
GCancellable *
ide_object_ref_cancellable (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  GCancellable *ret;

  g_return_val_if_fail (IDE_IS_OBJECT (self), NULL);

  ide_object_private_lock (priv);
  if (priv->cancellable == NULL)
    priv->cancellable = g_cancellable_new ();
  ret = g_object_ref (priv->cancellable);
  ide_object_private_unlock (priv);

  return g_steal_pointer (&ret);
}

/**
 * ide_object_get_parent:
 * @self: a #IdeObject
 *
 * Gets the parent #IdeObject, if any.
 *
 * This function may only be called from the main thread.
 *
 * Returns: (transfer none) (nullable): an #IdeObject or %NULL
 */
IdeObject *
ide_object_get_parent (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  IdeObject *ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_OBJECT (self), NULL);

  ide_object_private_lock (priv);
  ret = priv->parent;
  ide_object_private_unlock (priv);

  return g_steal_pointer (&ret);
}

/**
 * ide_object_ref_parent:
 * @self: a #IdeObject
 *
 * Gets the parent #IdeObject, if any.
 *
 * Returns: (transfer full) (nullable): an #IdeObject or %NULL
 */
IdeObject *
ide_object_ref_parent (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  IdeObject *ret;

  g_return_val_if_fail (IDE_IS_OBJECT (self), NULL);

  ide_object_private_lock (priv);
  ret = priv->parent ? g_object_ref (priv->parent) : NULL;
  ide_object_private_unlock (priv);

  return g_steal_pointer (&ret);
}

/**
 * ide_object_is_root:
 * @self: a #IdeObject
 *
 * Checks if @self is root, meaning it has no parent.
 *
 * Returns: %TRUE if @self has no parent
 */
gboolean
ide_object_is_root (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  gboolean ret;

  g_return_val_if_fail (IDE_IS_OBJECT (self), FALSE);

  ide_object_private_lock (priv);
  ret = priv->parent == NULL;
  ide_object_private_unlock (priv);

  return ret;
}

/**
 * ide_object_add:
 * @self: an #IdeObject
 * @sibling: (nullable): an #IdeObject or %NULL
 * @child: an #IdeObject
 * @location: location for child
 *
 * Adds @child to @self, with location dependent on @location.
 *
 * Generally, it is simpler to use the helper functions such as
 * ide_object_append(), ide_object_prepend(), ide_object_insert_before(),
 * or ide_object_insert_after().
 *
 * This function is primarily meant for consumers that don't know the
 * relative position they need until runtime.
 */
void
ide_object_add (IdeObject         *self,
                IdeObject         *sibling,
                IdeObject         *child,
                IdeObjectLocation  location)
{
  g_return_if_fail (IDE_IS_OBJECT (self));
  g_return_if_fail (IDE_IS_OBJECT (child));

  if (location == IDE_OBJECT_BEFORE_SIBLING ||
      location == IDE_OBJECT_AFTER_SIBLING)
    g_return_if_fail (IDE_IS_OBJECT (sibling));
  else
    g_return_if_fail (sibling == NULL);

  IDE_OBJECT_GET_CLASS (self)->add (self, sibling, child, location);
}

/**
 * ide_object_remove:
 * @self: an #IdeObject
 * @child: an #IdeObject
 *
 * Removes @child from @self.
 *
 * If @child is a borrowed reference, it may be finalized before this
 * function returns.
 */
void
ide_object_remove (IdeObject *self,
                   IdeObject *child)
{
  g_return_if_fail (IDE_IS_OBJECT (self));
  g_return_if_fail (IDE_IS_OBJECT (child));

  IDE_OBJECT_GET_CLASS (self)->remove (self, child);
}

/**
 * ide_object_append:
 * @self: an #IdeObject
 * @child: an #IdeObject
 *
 * Inserts @child as the last child of @self.
 */
void
ide_object_append (IdeObject *self,
                   IdeObject *child)
{
  ide_object_add (self, NULL, child, IDE_OBJECT_END);
}

/**
 * ide_object_prepend:
 * @self: an #IdeObject
 * @child: an #IdeObject
 *
 * Inserts @child as the first child of @self.
 */
void
ide_object_prepend (IdeObject *self,
                    IdeObject *child)
{
  ide_object_add (self, NULL, child, IDE_OBJECT_START);
}

/**
 * ide_object_insert_before:
 * @self: an #IdeObject
 * @sibling: an #IdeObject
 * @child: an #IdeObject
 *
 * Inserts @child into @self's children, directly before @sibling.
 *
 * @sibling MUST BE a child of @self.
 */
void
ide_object_insert_before (IdeObject *self,
                          IdeObject *sibling,
                          IdeObject *child)
{
  ide_object_add (self, sibling, child, IDE_OBJECT_BEFORE_SIBLING);
}

/**
 * ide_object_insert_after:
 * @self: an #IdeObject
 * @sibling: an #IdeObject
 * @child: an #IdeObject
 *
 * Inserts @child into @self's children, directly after @sibling.
 *
 * @sibling MUST BE a child of @self.
 */
void
ide_object_insert_after (IdeObject *self,
                         IdeObject *sibling,
                         IdeObject *child)
{
  ide_object_add (self, sibling, child, IDE_OBJECT_AFTER_SIBLING);
}

/**
 * ide_object_insert_sorted:
 * @self: a #IdeObject
 * @child: an #IdeObject
 * @func: (scope call): a #GCompareDataFunc that can be used to locate the
 *    proper sibling
 * @user_data: user data for @func
 *
 * Locates the proper sibling for @child by using @func amongst @self's
 * children #IdeObject. Those objects must already be sorted.
 */
void
ide_object_insert_sorted (IdeObject        *self,
                          IdeObject        *child,
                          GCompareDataFunc  func,
                          gpointer          user_data)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_return_if_fail (IDE_IS_OBJECT (self));
  g_return_if_fail (IDE_IS_OBJECT (child));
  g_return_if_fail (func != NULL);

  ide_object_lock (self);

  if (priv->children.length == 0)
    {
      ide_object_prepend (self, child);
      goto unlock;
    }

  g_assert (priv->children.head != NULL);
  g_assert (priv->children.tail != NULL);

  for (GList *iter = priv->children.head; iter; iter = iter->next)
    {
      IdeObject *other = iter->data;

      g_assert (IDE_IS_OBJECT (other));

      if (func (child, other, user_data) <= 0)
        {
          ide_object_insert_before (self, other, child);
          goto unlock;
        }
    }

  ide_object_append (self, child);

unlock:
  ide_object_unlock (self);
}

/**
 * ide_object_foreach:
 * @self: a #IdeObject
 * @callback: (scope call): a #GFunc to call for each child
 * @user_data: closure data for @callback
 *
 * Calls @callback for each child of @self.
 *
 * @callback is allowed to remove children from @self, but only as long as they are
 * the child passed to callback (or child itself). See g_queue_foreach() for more
 * details about what is allowed.
 */
void
ide_object_foreach (IdeObject *self,
                    GFunc      callback,
                    gpointer   user_data)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);

  g_return_if_fail (IDE_IS_OBJECT (self));
  g_return_if_fail (callback != NULL);

  ide_object_private_lock (priv);
  g_queue_foreach (&priv->children, callback, user_data);
  ide_object_private_unlock (priv);
}

static void
get_child_typed_cb (gpointer data,
                    gpointer user_data)
{
  IdeObject *child = data;
  GetChildTyped *q = user_data;

  if (q->child != NULL)
    return;

  if (G_TYPE_CHECK_INSTANCE_TYPE (child, q->type))
    q->child = g_object_ref (child);
}

/**
 * ide_object_get_child_typed:
 * @self: a #IdeObject
 * @type: the #GType of the child to match
 *
 * Finds the first child of @self that is of @type.
 *
 * Returns: (transfer full) (type IdeObject) (nullable): an #IdeObject or %NULL
 */
gpointer
ide_object_get_child_typed (IdeObject *self,
                            GType      type)
{
  GetChildTyped q = { type, NULL };

  g_return_val_if_fail (IDE_IS_OBJECT (self), NULL);
  g_return_val_if_fail (g_type_is_a (type, IDE_TYPE_OBJECT), NULL);

  ide_object_foreach (self, get_child_typed_cb, &q);

  return g_steal_pointer (&q.child);
}

static void
get_children_typed_cb (gpointer data,
                       gpointer user_data)
{
  IdeObject *child = data;
  GetChildrenTyped *q = user_data;

  if (G_TYPE_CHECK_INSTANCE_TYPE (child, q->type))
    g_ptr_array_add (q->array, g_object_ref (child));
}

/**
 * ide_object_get_children_typed:
 * @self: a #IdeObject
 * @type: a #GType
 *
 * Gets all children matching @type.
 *
 * Returns: (transfer full) (element-type IdeObject): a #GPtrArray of
 *   #IdeObject matching @type.
 */
GPtrArray *
ide_object_get_children_typed (IdeObject *self,
                               GType      type)
{
  g_autoptr(GPtrArray) ar = NULL;
  GetChildrenTyped q;

  g_return_val_if_fail (IDE_IS_OBJECT (self), NULL);
  g_return_val_if_fail (g_type_is_a (type, IDE_TYPE_OBJECT), NULL);

  ar = g_ptr_array_new ();

  q.type = type;
  q.array = ar;

  ide_object_foreach (self, get_children_typed_cb, &q);

  return g_steal_pointer (&ar);
}

/**
 * ide_object_ref_root:
 * @self: a #IdeObject
 *
 * Finds and returns the toplevel object in the tree.
 *
 * Returns: (transfer full): an #IdeObject
 */
IdeObject *
ide_object_ref_root (IdeObject *self)
{
  IdeObject *cur;

  g_return_val_if_fail (IDE_IS_OBJECT (self), NULL);

  cur = g_object_ref (self);

  while (!ide_object_is_root (cur))
    {
      IdeObject *tmp = cur;
      cur = ide_object_ref_parent (tmp);
      g_object_unref (tmp);
    }

  return g_steal_pointer (&cur);
}

static void
ide_object_async_init_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)object;
  g_autoptr(IdeObject) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_ASYNC_INITABLE (initable));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_OBJECT (self));

  if (!g_async_initable_init_finish (initable, result, &error))
    {
      g_warning ("Failed to initialize %s: %s",
                 G_OBJECT_TYPE_NAME (initable),
                 error->message);
      ide_object_destroy (IDE_OBJECT (initable));
    }
}

/**
 * ide_object_ensure_child_typed:
 * @self: a #IdeObject
 * @type: the #GType of the child
 *
 * Like ide_object_get_child_typed() except that it creates an object of
 * @type if it is missing.
 *
 * Returns: (transfer full) (nullable) (type IdeObject): an #IdeObject or %NULL
 */
gpointer
ide_object_ensure_child_typed (IdeObject *self,
                               GType      type)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  g_autoptr(IdeObject) ret = NULL;

  g_return_val_if_fail (IDE_IS_OBJECT (self), NULL);
  g_return_val_if_fail (g_type_is_a (type, IDE_TYPE_OBJECT), NULL);
  g_return_val_if_fail (!ide_object_in_destruction (self), NULL);

  ide_object_private_lock (priv);
  if (!(ret = ide_object_get_child_typed (self, type)))
    {
      g_autoptr(GError) error = NULL;

      ret = ide_object_new (type, self);

      if (G_IS_INITABLE (ret))
        {
          if (!g_initable_init (G_INITABLE (ret), NULL, &error))
            g_warning ("Failed to initialize %s: %s",
                       G_OBJECT_TYPE_NAME (ret), error->message);
        }
      else if (G_IS_ASYNC_INITABLE (ret))
        {
          g_async_initable_init_async (G_ASYNC_INITABLE (ret),
                                       G_PRIORITY_DEFAULT,
                                       priv->cancellable,
                                       ide_object_async_init_cb,
                                       g_object_ref (self));
        }
    }
  ide_object_private_unlock (priv);

  return g_steal_pointer (&ret);
}

/**
 * ide_object_destroyed:
 * @self: a #IdeObject
 *
 * This function sets *object_pointer to NULL if object_pointer != NULL. It's
 * intended to be used as a callback connected to the "destroy" signal of a
 * object. You connect ide_object_destroyed() as a signal handler, and pass the
 * address of your object variable as user data. Then when the object is
 * destroyed, the variable will be set to NULL. Useful for example to avoid
 * multiple copies of the same dialog.
 */
void
ide_object_destroyed (IdeObject **object_pointer)
{
  if (object_pointer != NULL)
    *object_pointer = NULL;
}

/* compat for now to ease porting */
void
ide_object_set_context (IdeObject  *object,
                        IdeContext *context)
{
  ide_object_append (IDE_OBJECT (context), object);
}

static gboolean dummy (gpointer p) { return G_SOURCE_REMOVE; }

/**
 * ide_object_get_context:
 * @object: a #IdeObject
 *
 * Gets the #IdeContext for the object.
 *
 * Returns: (transfer none) (nullable): an #IdeContext
 */
IdeContext *
ide_object_get_context (IdeObject *object)
{
  g_autoptr(IdeObject) root = ide_object_ref_root (object);
  IdeContext *ret = NULL;
  GSource *source;

  if (IDE_IS_CONTEXT (root))
    ret = IDE_CONTEXT (root);

  /* We can just return a borrowed instance if in main thread,
   * otherwise we need to queue the object to the main loop.
   */
  if (IDE_IS_MAIN_THREAD ())
    return ret;

  source = g_idle_source_new ();
  g_source_set_name (source, "context-release");
  g_source_set_callback (source, dummy, g_steal_pointer (&root), g_object_unref);
  g_source_attach (source, g_main_context_get_thread_default ());
  g_source_unref (source);

  return ret;
}

/**
 * ide_object_ref_context:
 * @self: a #IdeContext
 *
 * Gets the root #IdeContext for the object, if any.
 *
 * Returns: (transfer full) (nullable): an #IdeContext or %NULL
 */
IdeContext *
ide_object_ref_context (IdeObject *self)
{
  g_autoptr(IdeObject) root = NULL;

  g_return_val_if_fail (IDE_IS_OBJECT (self), NULL);

  if ((root = ide_object_ref_root (self)) && IDE_IS_CONTEXT (root))
    return IDE_CONTEXT (g_steal_pointer (&root));

  return NULL;
}

gboolean
ide_object_in_destruction (IdeObject *self)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  gboolean ret;

  g_return_val_if_fail (IDE_IS_OBJECT (self), FALSE);

  ide_object_lock (self);
  ret = priv->in_destruction || priv->destroyed;
  ide_object_unlock (self);

  return ret;
}

/**
 * ide_object_repr:
 * @self: a #IdeObject
 *
 * This function is similar to Python's `repr()` which gives a string
 * representation for the object. It is useful when debugging Builder
 * or when writing plugins.
 *
 * Returns: (transfer full): a string containing the string representation
 *   of the #IdeObject
 */
gchar *
ide_object_repr (IdeObject *self)
{
  g_autofree gchar *str = NULL;

  g_return_val_if_fail (IDE_IS_OBJECT (self), NULL);

  str = IDE_OBJECT_GET_CLASS (self)->repr (self);

  return g_strdup_printf ("<%s at %p>", str, self);
}

gboolean
ide_object_set_error_if_destroyed (IdeObject  *self,
                                   GError    **error)
{
  g_return_val_if_fail (IDE_IS_OBJECT (self), FALSE);

  if (ide_object_in_destruction (self))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CANCELLED,
                   "The object was destroyed");
      return TRUE;
    }

  return FALSE;
}

void
ide_object_log (gpointer        instance,
                GLogLevelFlags  level,
                const gchar    *domain,
                const gchar    *format,
                ...)
{
  g_autoptr(IdeObject) root = NULL;
  va_list args;

  g_assert (!instance || IDE_IS_OBJECT (instance));

  if G_UNLIKELY (instance == NULL || !(root = ide_object_ref_root (instance)))
    {
      va_start (args, format);
      g_logv (domain, level, format, args);
      va_end (args);
    }

  if (IDE_IS_CONTEXT (root))
    {
      g_autofree gchar *message = NULL;

      va_start (args, format);
      message = g_strdup_vprintf (format, args);
      ide_context_log (IDE_CONTEXT (root), level, domain, message);
      va_end (args);
    }
}

gboolean
ide_object_check_ready (IdeObject  *self,
                        GError    **error)
{
  IdeObjectPrivate *priv = ide_object_get_instance_private (self);
  g_autoptr(IdeObject) root = NULL;

  if (self == NULL ||
      priv->in_destruction ||
      priv->destroyed)
    goto failure;

  if (!(root = ide_object_ref_root (self)))
    goto failure;

  if (!IDE_IS_CONTEXT (root))
    goto failure;

  return TRUE;

failure:
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_CANCELLED,
               "Operation cancelled or in shutdown");

  return FALSE;
}
