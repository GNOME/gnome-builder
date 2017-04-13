/* egg-signal-group.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Garrett Regier <garrettregier@gmail.com>
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

#define G_LOG_DOMAIN "egg-signal-group"

#include <glib/gi18n.h>

#include "egg-signal-group.h"

/**
 * SECTION:eggsignalgroup
 * @title: EggSignalGroup
 * @short_description: Manage a collection of signals on a #GObject
 *
 * #EggSignalGroup manages to simplify the process of connecting
 * many signals to a #GObject as a group. As such there is no API
 * to disconnect a signal from the group.
 *
 * In particular, this allows you to:
 *
 *  - Change the target instance, which automatically causes disconnection
 *    of the signals from the old instance and connecting to the new instance.
 *  - Block and unblock signals as a group
 *  - Ensuring that blocked state transfers across target instances.
 *
 * One place you might want to use such a structure is with #GtkTextView and
 * #GtkTextBuffer. Often times, you'll need to connect to many signals on
 * #GtkTextBuffer from a #GtkTextView subclass. This allows you to create a
 * signal group during instance construction, simply bind the
 * #GtkTextView:buffer property to #EggSignalGroup:target and connect
 * all the signals you need. When the #GtkTextView:buffer property changes
 * all of the signals will be transitioned correctly.
 */

struct _EggSignalGroup
{
  GObject     parent_instance;

  GObject    *target;
  GPtrArray  *handlers;
  GType       target_type;
  gsize       block_count;
};

struct _EggSignalGroupClass
{
  GObjectClass parent_class;

  void (*bind)   (EggSignalGroup *self,
                  GObject        *target);
  void (*unbind) (EggSignalGroup *self,
                  GObject        *target);
};

typedef struct
{
  EggSignalGroup *group;
  gulong          handler_id;
  GClosure       *closure;
  GObject        *object;
  guint           signal_id;
  GQuark          signal_detail;
  guint           connect_after : 1;
} SignalHandler;

G_DEFINE_TYPE (EggSignalGroup, egg_signal_group, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_TARGET,
  PROP_TARGET_TYPE,
  LAST_PROP
};

enum {
  BIND,
  UNBIND,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

static void
egg_signal_group_set_target_type (EggSignalGroup *self,
                                  GType           target_type)
{
  g_assert (EGG_IS_SIGNAL_GROUP (self));
  g_assert (g_type_is_a (target_type, G_TYPE_OBJECT));

  self->target_type = target_type;

  /* The class must be created at least once for the signals
   * to be registered, otherwise g_signal_parse_name() will fail
   */
  if (G_TYPE_IS_INTERFACE (target_type))
    {
      if (g_type_default_interface_peek (target_type) == NULL)
        g_type_default_interface_unref (g_type_default_interface_ref (target_type));
    }
  else
    {
      if (g_type_class_peek (target_type) == NULL)
        g_type_class_unref (g_type_class_ref (target_type));
    }
}

static void
egg_signal_group__target_weak_notify (gpointer  data,
                                      GObject  *where_object_was)
{
  EggSignalGroup *self = data;
  gsize i;

  g_assert (EGG_IS_SIGNAL_GROUP (self));
  g_assert (where_object_was != NULL);
  g_assert (self->target == where_object_was);

  for (i = 0; i < self->handlers->len; i++)
    {
      SignalHandler *handler;

      handler = g_ptr_array_index (self->handlers, i);
      handler->handler_id = 0;
    }

  self->target = NULL;

  g_signal_emit (self, signals [UNBIND], 0);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TARGET]);
}

static void
egg_signal_group__connect_object_weak_notify (gpointer  data,
                                              GObject  *where_object_was)
{
  EggSignalGroup *self = data;
  gsize i;

  g_assert (EGG_IS_SIGNAL_GROUP (self));
  g_assert (where_object_was != NULL);

  for (i = 0; i < self->handlers->len; ++i)
    {
      SignalHandler *handler;

      handler = g_ptr_array_index (self->handlers, i);

      if (handler->object == where_object_was)
        {
          handler->object = NULL;
          g_ptr_array_remove_index_fast (self->handlers, i);
          return;
        }
    }

  g_critical ("Failed to find handler for %p", (void *)where_object_was);
}

static void
egg_signal_group_bind_handler (EggSignalGroup *self,
                               SignalHandler  *handler)
{
  gsize i;

  g_assert (self != NULL);
  g_assert (self->target != NULL);
  g_assert (handler != NULL);
  g_assert (handler->signal_id != 0);
  g_assert (handler->closure != NULL);
  g_assert (handler->handler_id == 0);

  handler->handler_id = g_signal_connect_closure_by_id (self->target,
                                                        handler->signal_id,
                                                        handler->signal_detail,
                                                        handler->closure,
                                                        handler->connect_after);

  g_assert (handler->handler_id != 0);

  for (i = 0; i < self->block_count; i++)
    g_signal_handler_block (self->target, handler->handler_id);
}

static void
egg_signal_group_bind (EggSignalGroup *self,
                       GObject        *target)
{
  gsize i;

  g_assert (EGG_IS_SIGNAL_GROUP (self));
  g_assert (self->target == NULL);
  g_assert (!target || G_IS_OBJECT (target));

  if (target == NULL)
    return;

  self->target = target;
  g_object_weak_ref (self->target,
                     egg_signal_group__target_weak_notify,
                     self);

  g_object_ref (target);

  for (i = 0; i < self->handlers->len; i++)
    {
      SignalHandler *handler;

      handler = g_ptr_array_index (self->handlers, i);
      egg_signal_group_bind_handler (self, handler);
    }

  g_signal_emit (self, signals [BIND], 0, target);
  g_object_unref (target);
}

static void
egg_signal_group_unbind (EggSignalGroup *self)
{
  GObject *target;
  gsize i;

  g_return_if_fail (EGG_IS_SIGNAL_GROUP (self));

  if (self->target == NULL)
    return;

  target = self->target;
  self->target = NULL;

  g_object_weak_unref (target,
                       egg_signal_group__target_weak_notify,
                       self);

  for (i = 0; i < self->handlers->len; i++)
    {
      SignalHandler *handler;
      gulong handler_id;

      handler = g_ptr_array_index (self->handlers, i);

      g_assert (handler != NULL);
      g_assert (handler->signal_id != 0);
      g_assert (handler->closure != NULL);
      g_assert (handler->handler_id != 0);

      handler_id = handler->handler_id;
      handler->handler_id = 0;

      g_signal_handler_disconnect (target, handler_id);
    }

  g_signal_emit (self, signals [UNBIND], 0);
}

static gboolean
egg_signal_group_check_target_type (EggSignalGroup *self,
                                    gpointer        target)
{
  if ((target != NULL) &&
      !g_type_is_a (G_OBJECT_TYPE (target), self->target_type))
    {
      g_critical ("Failed to set EggSignalGroup of target type %s "
                  "using target %p of type %s",
                  g_type_name (self->target_type),
                  target, G_OBJECT_TYPE_NAME (target));
      return FALSE;
    }

  return TRUE;
}

/**
 * egg_signal_group_block:
 * @self: the #EggSignalGroup
 *
 * Blocks all signal handlers managed by @self so they will not
 * be called during any signal emissions. Must be unblocked exactly
 * the same number of times it has been blocked to become active again.
 *
 * This blocked state will be kept across changes of the target instance.
 *
 * See: g_signal_handler_block().
 */
void
egg_signal_group_block (EggSignalGroup *self)
{
  gsize i;

  g_return_if_fail (EGG_IS_SIGNAL_GROUP (self));
  g_return_if_fail (self->block_count != G_MAXSIZE);

  self->block_count++;

  if (self->target == NULL)
    return;

  for (i = 0; i < self->handlers->len; i++)
    {
      SignalHandler *handler;

      handler = g_ptr_array_index (self->handlers, i);

      g_assert (handler != NULL);
      g_assert (handler->signal_id != 0);
      g_assert (handler->closure != NULL);
      g_assert (handler->handler_id != 0);

      g_signal_handler_block (self->target, handler->handler_id);
    }
}

/**
 * egg_signal_group_unblock:
 * @self: the #EggSignalGroup
 *
 * Unblocks all signal handlers managed by @self so they will be
 * called again during any signal emissions unless it is blocked
 * again. Must be unblocked exactly the same number of times it
 * has been blocked to become active again.
 *
 * See: g_signal_handler_unblock().
 */
void
egg_signal_group_unblock (EggSignalGroup *self)
{
  gsize i;

  g_return_if_fail (EGG_IS_SIGNAL_GROUP (self));
  g_return_if_fail (self->block_count != 0);

  self->block_count--;

  if (self->target == NULL)
    return;

  for (i = 0; i < self->handlers->len; i++)
    {
      SignalHandler *handler;

      handler = g_ptr_array_index (self->handlers, i);

      g_assert (handler != NULL);
      g_assert (handler->signal_id != 0);
      g_assert (handler->closure != NULL);
      g_assert (handler->handler_id != 0);

      g_signal_handler_unblock (self->target, handler->handler_id);
    }
}

/**
 * egg_signal_group_get_target:
 * @self: the #EggSignalGroup
 *
 * Gets the target instance used when connecting signals.
 *
 * Returns: (nullable) (transfer none) (type GObject): The target instance.
 */
gpointer
egg_signal_group_get_target (EggSignalGroup *self)
{
  g_return_val_if_fail (EGG_IS_SIGNAL_GROUP (self), NULL);

  return (gpointer)self->target;
}

/**
 * egg_signal_group_set_target:
 * @self: the #EggSignalGroup.
 * @target: (nullable) (type GObject): The target instance used
 *     when connecting signals.
 *
 * Sets the target instance used when connecting signals. Any signal
 * that has been registered with egg_signal_group_connect_object() or
 * similar functions will be connected to this object.
 *
 * If the target instance was previously set, signals will be
 * disconnected from that object prior to connecting to @target.
 */
void
egg_signal_group_set_target (EggSignalGroup *self,
                             gpointer        target)
{
  g_return_if_fail (EGG_IS_SIGNAL_GROUP (self));

  if (target == (gpointer)self->target)
    return;

  if (!egg_signal_group_check_target_type (self, target))
    return;

  egg_signal_group_unbind (self);
  egg_signal_group_bind (self, target);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TARGET]);
}

static void
signal_handler_free (gpointer data)
{
  SignalHandler *handler = data;

  if (handler->object != NULL)
    {
      g_object_weak_unref (handler->object,
                           egg_signal_group__connect_object_weak_notify,
                           handler->group);
      handler->object = NULL;
    }

  g_clear_pointer (&handler->closure, g_closure_unref);
  handler->handler_id = 0;
  handler->signal_id = 0;
  handler->signal_detail = 0;
  g_slice_free (SignalHandler, handler);
}

static void
egg_signal_group_constructed (GObject *object)
{
  EggSignalGroup *self = (EggSignalGroup *)object;

  if (!egg_signal_group_check_target_type (self, self->target))
    egg_signal_group_set_target (self, NULL);

  G_OBJECT_CLASS (egg_signal_group_parent_class)->constructed (object);
}

static void
egg_signal_group_dispose (GObject *object)
{
  EggSignalGroup *self = (EggSignalGroup *)object;

  egg_signal_group_unbind (self);
  g_clear_pointer (&self->handlers, g_ptr_array_unref);

  G_OBJECT_CLASS (egg_signal_group_parent_class)->dispose (object);
}

static void
egg_signal_group_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EggSignalGroup *self = EGG_SIGNAL_GROUP (object);

  switch (prop_id)
    {
    case PROP_TARGET:
      g_value_set_object (value, egg_signal_group_get_target (self));
      break;

    case PROP_TARGET_TYPE:
      g_value_set_gtype (value, self->target_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_signal_group_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EggSignalGroup *self = EGG_SIGNAL_GROUP (object);

  switch (prop_id)
    {
    case PROP_TARGET:
      egg_signal_group_set_target (self, g_value_get_object (value));
      break;

    case PROP_TARGET_TYPE:
      egg_signal_group_set_target_type (self, g_value_get_gtype (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_signal_group_class_init (EggSignalGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = egg_signal_group_constructed;
  object_class->dispose = egg_signal_group_dispose;
  object_class->get_property = egg_signal_group_get_property;
  object_class->set_property = egg_signal_group_set_property;

  /**
   * EggSignalGroup:target
   *
   * The target instance used when connecting signals.
   */
  properties [PROP_TARGET] =
    g_param_spec_object ("target",
                         "Target",
                         "The target instance used when connecting signals.",
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * EggSignalGroup:target-type
   *
   * The GType of the target property.
   */
  properties [PROP_TARGET_TYPE] =
    g_param_spec_gtype ("target-type",
                        "Target Type",
                        "The GType of the target property.",
                        G_TYPE_OBJECT,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * EggSignalGroup::bind:
   * @self: the #EggSignalGroup
   * @instance: a #GObject
   *
   * This signal is emitted when the target instance of @self
   * is set to a new #GObject.
   *
   * This signal will only be emitted if the target of @self is non-%NULL.
   */
  signals [BIND] =
    g_signal_new ("bind",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_OBJECT);

  /**
   * EggSignalGroup::unbind:
   * @self: a #EggSignalGroup
   *
   * This signal is emitted when the target instance of @self
   * is set to a new #GObject.
   *
   * This signal will only be emitted if the previous target
   * of @self is non-%NULL.
   */
  signals [UNBIND] =
    g_signal_new ("unbind",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
}

static void
egg_signal_group_init (EggSignalGroup *self)
{
  self->handlers = g_ptr_array_new_with_free_func (signal_handler_free);
  self->target_type = G_TYPE_OBJECT;
}

/**
 * egg_signal_group_new:
 * @target_type: the #GType of the target instance.
 *
 * Creates a new #EggSignalGroup for target instances of @target_type.
 *
 * Returns: a new #EggSignalGroup
 */
EggSignalGroup *
egg_signal_group_new (GType target_type)
{
  g_return_val_if_fail (g_type_is_a (target_type, G_TYPE_OBJECT), NULL);

  return g_object_new (EGG_TYPE_SIGNAL_GROUP,
                       "target-type", target_type,
                       NULL);
}

static void
egg_signal_group_connect_full (EggSignalGroup *self,
                               const gchar    *detailed_signal,
                               GCallback       callback,
                               gpointer        data,
                               GClosureNotify  notify,
                               GConnectFlags   flags,
                               gboolean        is_object)
{
  SignalHandler *handler;
  GClosure *closure;
  guint signal_id;
  GQuark signal_detail;

  g_return_if_fail (EGG_IS_SIGNAL_GROUP (self));
  g_return_if_fail (detailed_signal != NULL);
  g_return_if_fail (g_signal_parse_name (detailed_signal, self->target_type,
                                         &signal_id, &signal_detail, TRUE) != 0);
  g_return_if_fail (callback != NULL);

  if ((flags & G_CONNECT_SWAPPED) != 0)
    closure = g_cclosure_new_swap (callback, data, notify);
  else
    closure = g_cclosure_new (callback, data, notify);

  handler = g_slice_new0 (SignalHandler);
  handler->group = self;
  handler->signal_id = signal_id;
  handler->signal_detail = signal_detail;
  handler->closure = g_closure_ref (closure);
  handler->connect_after = ((flags & G_CONNECT_AFTER) != 0);

  g_closure_sink (closure);

  if (is_object)
    {
      /* This is what g_cclosure_new_object() does */
      g_object_watch_closure (data, closure);

      handler->object = data;
      g_object_weak_ref (data,
                         egg_signal_group__connect_object_weak_notify,
                         self);
    }

  g_ptr_array_add (self->handlers, handler);

  if (self->target != NULL)
    egg_signal_group_bind_handler (self, handler);
}

/**
 * egg_signal_group_connect_object: (skip)
 * @self: a #EggSignalGroup
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: (scope notified): the #GCallback to connect
 * @object: the #GObject to pass as data to @callback calls
 *
 * Connects @callback to the signal @detailed_signal
 * on the target object of @self.
 *
 * Ensures that the @object stays alive during the call to @callback
 * by temporarily adding a reference count. When the @object is destroyed
 * the signal handler will automatically be removed.
 *
 * See: g_signal_connect_object().
 */
void
egg_signal_group_connect_object (EggSignalGroup *self,
                                 const gchar    *detailed_signal,
                                 GCallback       c_handler,
                                 gpointer        object,
                                 GConnectFlags   flags)
{
  g_return_if_fail (G_IS_OBJECT (object));

  egg_signal_group_connect_full (self, detailed_signal, c_handler, object, NULL,
                                 flags, TRUE);
}

/**
 * egg_signal_group_connect_data:
 * @self: a #EggSignalGroup
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: (scope notified) (closure data) (destroy notify): the #GCallback to connect
 * @data: the data to pass to @callback calls
 * @notify: function to be called when disposing of @self
 * @flags: the flags used to create the signal connection
 *
 * Connects @callback to the signal @detailed_signal
 * on the target instance of @self.
 *
 * See: g_signal_connect_data().
 */
void
egg_signal_group_connect_data (EggSignalGroup *self,
                               const gchar    *detailed_signal,
                               GCallback       c_handler,
                               gpointer        data,
                               GClosureNotify  notify,
                               GConnectFlags   flags)
{
  egg_signal_group_connect_full (self, detailed_signal, c_handler, data, notify,
                                 flags, FALSE);
}

/**
 * egg_signal_group_connect: (skip)
 * @self: a #EggSignalGroup
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: (scope notified): the #GCallback to connect
 * @data: the data to pass to @callback calls
 *
 * Connects @callback to the signal @detailed_signal
 * on the target instance of @self.
 *
 * See: g_signal_connect().
 */
void
egg_signal_group_connect (EggSignalGroup *self,
                          const gchar    *detailed_signal,
                          GCallback       c_handler,
                          gpointer        data)
{
  egg_signal_group_connect_full (self, detailed_signal, c_handler, data, NULL,
                                 0, FALSE);
}

/**
 * egg_signal_group_connect_after: (skip)
 * @self: a #EggSignalGroup
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: (scope notified): the #GCallback to connect
 * @data: the data to pass to @callback calls
 *
 * Connects @callback to the signal @detailed_signal
 * on the target instance of @self.
 *
 * The @callback will be called after the default handler of the signal.
 *
 * See: g_signal_connect_after().
 */
void
egg_signal_group_connect_after (EggSignalGroup *self,
                                const gchar    *detailed_signal,
                                GCallback       c_handler,
                                gpointer        data)
{
  egg_signal_group_connect_full (self, detailed_signal, c_handler,
                                 data, NULL, G_CONNECT_AFTER, FALSE);
}

/**
 * egg_signal_group_connect_swapped:
 * @self: a #EggSignalGroup
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: (scope async): the #GCallback to connect
 * @data: the data to pass to @callback calls
 *
 * Connects @callback to the signal @detailed_signal
 * on the target instance of @self.
 *
 * The instance on which the signal is emitted and @data
 * will be swapped when calling @callback.
 *
 * See: g_signal_connect_swapped().
 */
void
egg_signal_group_connect_swapped (EggSignalGroup *self,
                                  const gchar    *detailed_signal,
                                  GCallback       c_handler,
                                  gpointer        data)
{
  egg_signal_group_connect_full (self, detailed_signal, c_handler, data, NULL,
                                 G_CONNECT_SWAPPED, FALSE);
}
