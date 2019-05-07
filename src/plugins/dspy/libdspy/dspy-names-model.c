/* dspy-names-model.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define G_LOG_DOMAIN "dspy-names-model"

#include "config.h"

#include "dspy-names-model.h"
#include "dspy-private.h"

struct _DspyNamesModel
{
  GObject          parent_instance;
  DspyConnection  *connection;
  GSequence       *items;
  GDBusConnection *bus;
  guint            name_owner_changed_handler;
};

enum {
  PROP_0,
  PROP_CONNECTION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
_g_weak_ref_free (GWeakRef *wr)
{
  g_weak_ref_clear (wr);
  g_slice_free (GWeakRef, wr);
}

static GType
dspy_names_model_get_item_type (GListModel *model)
{
  return DSPY_TYPE_NAME;
}

static guint
dspy_names_model_get_n_items (GListModel *model)
{
  return g_sequence_get_length (DSPY_NAMES_MODEL (model)->items);
}

static gpointer
dspy_names_model_get_item (GListModel *model,
                           guint       position)
{
  DspyNamesModel *self = DSPY_NAMES_MODEL (model);
  GSequenceIter *iter;

  g_assert (DSPY_IS_NAMES_MODEL (self));

  if ((iter = g_sequence_get_iter_at_pos (self->items, position)) &&
      !g_sequence_iter_is_end (iter))
    return g_object_ref (g_sequence_get (iter));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = dspy_names_model_get_item_type;
  iface->get_n_items = dspy_names_model_get_n_items;
  iface->get_item = dspy_names_model_get_item;
}

/**
 * dspy_names_model_get_by_name:
 * @self: a #DspyNamesModel
 * @name: the name to lookup, such as ":1.0" or "org.freedesktop.DBus"
 *
 * Looks for a #DspyName that matches @name.
 *
 * Returns: (transfer full) (nullable): a #DspyName or %NULL
 */
DspyName *
dspy_names_model_get_by_name (DspyNamesModel *self,
                              const gchar    *name)
{
  g_autoptr(DspyName) tmp = NULL;
  GSequenceIter *iter;

  g_assert (DSPY_IS_NAMES_MODEL (self));
  g_assert (name != NULL);

  tmp = dspy_name_new (self->connection, name, FALSE);
  iter = g_sequence_lookup (self->items, tmp, (GCompareDataFunc) dspy_name_compare, NULL);

  if (!iter || g_sequence_iter_is_end (iter))
    return NULL;

  return g_object_ref (g_sequence_get (iter));
}

static void
dspy_names_model_add_names (DspyNamesModel      *self,
                            GDBusConnection     *bus,
                            const gchar * const *names,
                            gboolean             is_activatable)
{
  g_assert (DSPY_IS_NAMES_MODEL (self));
  g_assert (names != NULL);

  for (guint i = 0; names[i] != NULL; i++)
    {
      g_autoptr(DspyName) name = NULL;
      GSequenceIter *iter;

      if ((name = dspy_names_model_get_by_name (self, names[i])))
        {
          if (is_activatable && !dspy_name_get_activatable (name))
            _dspy_name_set_activatable (name, TRUE);
          continue;
        }

      name = dspy_name_new (self->connection, names[i], is_activatable);

      _dspy_name_refresh_pid (name, bus);
      _dspy_name_refresh_owner (name, bus);

      iter = g_sequence_insert_sorted (self->items,
                                       g_steal_pointer (&name),
                                       (GCompareDataFunc) dspy_name_compare,
                                       NULL);
      g_list_model_items_changed (G_LIST_MODEL (self),
                                  g_sequence_iter_get_position (iter),
                                  0, 1);
    }
}

static void
dspy_names_model_name_owner_changed_cb (GDBusConnection *connection,
                                        const gchar     *sender_name,
                                        const gchar     *object_path,
                                        const gchar     *interface_name,
                                        const gchar     *signal_name,
                                        GVariant        *params,
                                        gpointer         user_data)
{
  GWeakRef *wr = user_data;
  g_autoptr(DspyNamesModel) self = NULL;
  g_autoptr(DspyName) name = NULL;
  GSequenceIter *seq;
  const gchar *vname;
  const gchar *vold_name;
  const gchar *vnew_name;

  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (params != NULL);
  g_assert (g_variant_is_of_type (params, G_VARIANT_TYPE ("(sss)")));
  g_assert (wr != NULL);

  if (!(self = g_weak_ref_get (wr)))
    return;

  g_variant_get (params, "(&s&s&s)", &vname, &vold_name, &vnew_name);

  name = dspy_name_new (self->connection, vname, FALSE);
  seq = g_sequence_lookup (self->items,
                           name,
                           (GCompareDataFunc) dspy_name_compare,
                           NULL);

  if (seq == NULL)
    {
      if (vnew_name[0])
        {
          const gchar *names[] = { vname, NULL };
          dspy_names_model_add_names (self, connection, names, FALSE);
        }
    }
  else if (!vnew_name[0])
    {
      DspyName *item = g_sequence_get (seq);

      if (dspy_name_get_activatable (item) &&
          dspy_name_get_name (item)[0] != ':')
        {
          _dspy_name_clear_pid (item);
          _dspy_name_set_owner (item, NULL);
        }
      else
        {
          guint position = g_sequence_iter_get_position (seq);
          g_sequence_remove (seq);
          g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
        }
    }
  else
    {
      DspyName *item = g_sequence_get (seq);

      if (vnew_name[0] == ':')
        _dspy_name_set_owner (item, vnew_name);

      _dspy_name_refresh_pid (item, connection);
    }
}

static void
dspy_names_model_init_list_activatable_names_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  GDBusConnection *bus = (GDBusConnection *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  gint *n_active;

  g_assert (G_IS_DBUS_CONNECTION (bus));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if ((reply = g_dbus_connection_call_finish (bus, result, &error)))
    {
      g_autofree const gchar **names = NULL;
      DspyNamesModel *self;

      g_assert (reply != NULL);
      g_assert (g_variant_is_of_type (reply, G_VARIANT_TYPE ("(as)")));

      self = g_task_get_source_object (task);
      g_variant_get (reply, "(^as)", &names);
      dspy_names_model_add_names (self, bus, (const gchar * const *)names, TRUE);
    }

  n_active = g_task_get_task_data (task);
  g_assert (n_active != NULL);
  g_assert (*n_active > 0);

  if (--(*n_active) == 0)
    g_task_return_boolean (task, TRUE);
}

static void
dspy_names_model_init_list_names_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GDBusConnection *bus = (GDBusConnection *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  gint *n_active;

  g_assert (G_IS_DBUS_CONNECTION (bus));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if ((reply = g_dbus_connection_call_finish (bus, result, &error)))
    {
      g_autofree const gchar **names = NULL;
      DspyNamesModel *self;

      g_assert (reply != NULL);
      g_assert (g_variant_is_of_type (reply, G_VARIANT_TYPE ("(as)")));

      self = g_task_get_source_object (task);
      g_variant_get (reply, "(^as)", &names);
      dspy_names_model_add_names (self, bus, (const gchar * const *)names, FALSE);
    }

  n_active = g_task_get_task_data (task);
  g_assert (n_active != NULL);
  g_assert (*n_active > 0);

  if (--(*n_active) == 0)
    g_task_return_boolean (task, TRUE);
}

static void
dspy_names_model_init_open_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  DspyConnection *connection = (DspyConnection *)object;
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  DspyNamesModel *self;
  GWeakRef *wr;

  g_assert (DSPY_IS_CONNECTION (connection));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!(bus = dspy_connection_open_finish (connection, result, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = g_task_get_source_object (task);

  g_assert (self != NULL);
  g_assert (DSPY_IS_NAMES_MODEL (self));

  self->bus = g_object_ref (bus);

  /* Because g_dbus_connection_signal_subscribe() is not guaranteed to
   * call the cleanup function synchronously when unsubscribed, we need to
   * use a weak ref in allocated state to ensure that we do not have a
   * reference cycle. Otherwise, calling unsubscribe() from our finalize
   * handler could result in a use-after-free. And we can't use a full
   * reference because we'd never dispose/finalize without external
   * intervention.
   */
  wr = g_slice_new0 (GWeakRef);
  g_weak_ref_init (wr, self);
  self->name_owner_changed_handler =
    g_dbus_connection_signal_subscribe (bus,
                                        NULL,
                                        "org.freedesktop.DBus",
                                        "NameOwnerChanged",
                                        NULL,
                                        NULL,
                                        0,
                                        dspy_names_model_name_owner_changed_cb,
                                        g_steal_pointer (&wr),
                                        (GDestroyNotify)_g_weak_ref_free);

  g_dbus_connection_call (bus,
                          "org.freedesktop.DBus",
                          "/org/freedesktop/DBus",
                          "org.freedesktop.DBus",
                          "ListActivatableNames",
                          g_variant_new ("()"),
                          G_VARIANT_TYPE ("(as)"),
                          G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
                          G_MAXINT,
                          g_task_get_cancellable (task),
                          dspy_names_model_init_list_activatable_names_cb,
                          g_object_ref (task));

  g_dbus_connection_call (bus,
                          "org.freedesktop.DBus",
                          "/org/freedesktop/DBus",
                          "org.freedesktop.DBus",
                          "ListNames",
                          g_variant_new ("()"),
                          G_VARIANT_TYPE ("(as)"),
                          G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
                          G_MAXINT,
                          g_task_get_cancellable (task),
                          dspy_names_model_init_list_names_cb,
                          g_object_ref (task));
}

static void
dspy_names_model_init_async (GAsyncInitable      *initable,
                             gint                 io_priority,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  DspyNamesModel *self = (DspyNamesModel *)initable;
  g_autoptr(GTask) task = NULL;
  gint n_active = 2;

  g_assert (DSPY_IS_NAMES_MODEL (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, dspy_names_model_init_async);
  g_task_set_task_data (task, g_memdup (&n_active, sizeof n_active), g_free);

  if (self->connection == NULL)
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_INITIALIZED,
                             "No connection to introspect");
  else
    dspy_connection_open_async (self->connection,
                                cancellable,
                                dspy_names_model_init_open_cb,
                                g_steal_pointer (&task));
}

static gboolean
dspy_names_model_init_finish (GAsyncInitable  *initable,
                              GAsyncResult    *result,
                              GError         **error)
{
  g_assert (DSPY_IS_NAMES_MODEL (initable));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = dspy_names_model_init_async;
  iface->init_finish = dspy_names_model_init_finish;
}

G_DEFINE_TYPE_WITH_CODE (DspyNamesModel, dspy_names_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

/**
 * dspy_names_model_new:
 * @connection: a #DspyConnection
 *
 * Create a new #DspyNamesModel.
 *
 * Returns: (transfer full): a newly created #DspyNamesModel
 */
DspyNamesModel *
dspy_names_model_new (DspyConnection *connection)
{
  return g_object_new (DSPY_TYPE_NAMES_MODEL,
                       "connection", connection,
                       NULL);
}

static void
dspy_names_model_dispose (GObject *object)
{
  DspyNamesModel *self = (DspyNamesModel *)object;

  g_assert (DSPY_IS_NAMES_MODEL (self));
  g_assert (self->name_owner_changed_handler == 0 || self->bus != NULL);

  if (self->name_owner_changed_handler > 0)
    {
      guint handler_id = self->name_owner_changed_handler;
      self->name_owner_changed_handler = 0;
      g_dbus_connection_signal_unsubscribe (self->bus, handler_id);
    }

  g_clear_object (&self->bus);

  G_OBJECT_CLASS (dspy_names_model_parent_class)->dispose (object);
}

static void
dspy_names_model_finalize (GObject *object)
{
  DspyNamesModel *self = (DspyNamesModel *)object;

  g_clear_pointer (&self->items, g_sequence_free);
  g_clear_object (&self->connection);

  G_OBJECT_CLASS (dspy_names_model_parent_class)->finalize (object);
}

static void
dspy_names_model_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  DspyNamesModel *self = DSPY_NAMES_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_names_model_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  DspyNamesModel *self = DSPY_NAMES_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      self->connection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_names_model_class_init (DspyNamesModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = dspy_names_model_dispose;
  object_class->finalize = dspy_names_model_finalize;
  object_class->get_property = dspy_names_model_get_property;
  object_class->set_property = dspy_names_model_set_property;

  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection",
                         "Connection",
                         "The connection to introspect",
                         DSPY_TYPE_CONNECTION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
dspy_names_model_init (DspyNamesModel *self)
{
  self->items = g_sequence_new (g_object_unref);
}
