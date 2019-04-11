/* dspy-connection.c
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

#define G_LOG_DOMAIN "dspy-connection"

#include "config.h"

#include "dspy-connection.h"
#include "dspy-names-model.h"

struct _DspyConnection
{
  GObject          parent_instance;
  GCancellable    *cancellable;
  GDBusConnection *connection;
  gchar           *address;
  gchar           *connected_address;
  GPtrArray       *errors;
  GBusType         bus_type;
};

G_DEFINE_TYPE (DspyConnection, dspy_connection, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_BUS_TYPE,
  PROP_CONNECTION,
  PROP_HAS_ERROR,
  N_PROPS
};

enum {
  ERROR,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

/**
 * dspy_connection_new_for_address:
 * @address: an address to connect to the bus
 *
 * Create a new #DspyConnection.
 *
 * Returns: (transfer full): a newly created #DspyConnection
 */
DspyConnection *
dspy_connection_new_for_address (const gchar *address)
{
  return g_object_new (DSPY_TYPE_CONNECTION,
                       "address", address,
                       NULL);
}

/**
 * dspy_connection_new_for_bus:
 * @bus_type: the type of bus connection
 *
 * Create a new #DspyConnection.
 *
 * Returns: (transfer full): a newly created #DspyConnection
 */
DspyConnection *
dspy_connection_new_for_bus (GBusType bus_type)
{
  return g_object_new (DSPY_TYPE_CONNECTION,
                       "bus-type", bus_type,
                       NULL);
}

static void
dspy_connection_dispose (GObject *object)
{
  DspyConnection *self = (DspyConnection *)object;

  g_assert (DSPY_IS_CONNECTION (self));

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (self->connection != NULL)
    {
      if (!g_dbus_connection_is_closed (self->connection))
        g_dbus_connection_close (self->connection, NULL, NULL, NULL);
      g_clear_object (&self->connection);
    }

  G_OBJECT_CLASS (dspy_connection_parent_class)->dispose (object);
}

static void
dspy_connection_finalize (GObject *object)
{
  DspyConnection *self = (DspyConnection *)object;

  g_clear_pointer (&self->address, g_free);
  g_clear_pointer (&self->connected_address, g_free);

  G_OBJECT_CLASS (dspy_connection_parent_class)->finalize (object);
}

static void
dspy_connection_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  DspyConnection *self = DSPY_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      g_value_set_string (value, dspy_connection_get_address (self));
      break;

    case PROP_BUS_TYPE:
      g_value_set_enum (value, dspy_connection_get_bus_type (self));
      break;

    case PROP_CONNECTION:
      g_value_set_object (value, dspy_connection_get_connection (self));
      break;

    case PROP_HAS_ERROR:
      g_value_set_boolean (value, dspy_connection_get_has_error (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_connection_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  DspyConnection *self = DSPY_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      if (g_value_get_string (value))
        {
          self->address = g_value_dup_string (value);
          self->bus_type = G_BUS_TYPE_NONE;
        }
      break;

    case PROP_BUS_TYPE:
      if (g_value_get_enum (value))
        {
          self->bus_type = g_value_get_enum (value);
          g_clear_pointer (&self->address, g_free);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_connection_class_init (DspyConnectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = dspy_connection_dispose;
  object_class->finalize = dspy_connection_finalize;
  object_class->get_property = dspy_connection_get_property;
  object_class->set_property = dspy_connection_set_property;

  properties [PROP_ADDRESS] =
    g_param_spec_string ("address",
                         "Address",
                         "The bus address to connect",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_BUS_TYPE] =
    g_param_spec_enum ("bus-type",
                       "Bus Type",
                       "The bus type to connect to, if no address is specified",
                       G_TYPE_BUS_TYPE,
                       G_BUS_TYPE_NONE,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection",
                         "Connection",
                         "The underlying GDBus connection",
                         G_TYPE_DBUS_CONNECTION,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HAS_ERROR] =
    g_param_spec_boolean ("has-error",
                          "Has Error",
                          "Has Error",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [ERROR] =
    g_signal_new ("error",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, G_TYPE_ERROR | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
dspy_connection_init (DspyConnection *self)
{
}

/**
 * dspy_connection_get_connection:
 *
 * Gets the #GDBusConnection, if one has been opened.
 *
 * Returns: (transfer none) (nullable): a #GDBusConnection or %NULL
 */
GDBusConnection *
dspy_connection_get_connection (DspyConnection *self)
{
  g_return_val_if_fail (DSPY_IS_CONNECTION (self), NULL);

  return self->connection;
}

const gchar *
dspy_connection_get_address (DspyConnection *self)
{
  g_return_val_if_fail (DSPY_IS_CONNECTION (self), NULL);

  if (self->address)
    return self->address;

  if (self->connected_address)
    return self->connected_address;

  return NULL;
}

GBusType
dspy_connection_get_bus_type (DspyConnection *self)
{
  g_return_val_if_fail (DSPY_IS_CONNECTION (self), G_BUS_TYPE_NONE);

  return self->bus_type;
}

static void
dspy_connection_open_address_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!(bus = g_dbus_connection_new_for_address_finish (result, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&bus), g_object_unref);
}

void
dspy_connection_open_async (DspyConnection      *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (DSPY_IS_CONNECTION (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, dspy_connection_open_async);

  if (self->connection != NULL)
    {
      g_task_return_pointer (task, g_object_ref (self->connection), g_object_unref);
      return;
    }

  g_clear_pointer (&self->connected_address, g_free);

  if (self->address != NULL)
    self->connected_address = g_strdup (self->address);
  else
    self->connected_address = g_dbus_address_get_for_bus_sync (self->bus_type,
                                                               cancellable,
                                                               &error);

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_dbus_connection_new_for_address (self->connected_address,
                                       (G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION |
                                        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT),
                                       NULL,
                                       cancellable,
                                       dspy_connection_open_address_cb,
                                       g_steal_pointer (&task));
}

/**
 * dspy_connection_open_finish:
 *
 * Completes an asynchronous request to dspy_connection_open_async().
 *
 * Returns: (transfer full): a #GDBusConnection if successful; otherwise
 *   %NULL and @error is set.
 */
GDBusConnection *
dspy_connection_open_finish (DspyConnection  *self,
                             GAsyncResult    *result,
                             GError         **error)
{
  GDBusConnection *bus;

  g_return_val_if_fail (DSPY_IS_CONNECTION (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  if ((bus = g_task_propagate_pointer (G_TASK (result), error)))
    {
      g_dbus_connection_set_exit_on_close (bus, FALSE);

      if (g_set_object (&self->connection, bus))
        g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONNECTION]);
    }

  return g_steal_pointer (&bus);
}

void
dspy_connection_close (DspyConnection *self)
{
  g_return_if_fail (DSPY_IS_CONNECTION (self));

  g_cancellable_cancel (self->cancellable);
  g_dbus_connection_close (self->connection, NULL, NULL, NULL);

  g_clear_object (&self->connection);
  g_clear_object (&self->cancellable);
}

static void
dspy_connection_list_names_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  DspyConnection *self;

  g_assert (G_IS_ASYNC_INITABLE (initable));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);

  if (!g_async_initable_init_finish (initable, result, &error))
    {
      dspy_connection_add_error (self, error);
      g_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      dspy_connection_clear_errors (self);
      g_task_return_pointer (task, g_object_ref (initable), g_object_unref);
    }
}

void
dspy_connection_list_names_async (DspyConnection      *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(DspyNamesModel) model = NULL;

  g_return_if_fail (DSPY_IS_CONNECTION (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, dspy_connection_list_names_async);

  model = dspy_names_model_new (self);

  g_async_initable_init_async (G_ASYNC_INITABLE (model),
                               G_PRIORITY_DEFAULT,
                               cancellable,
                               dspy_connection_list_names_cb,
                               g_steal_pointer (&task));
}

GListModel *
dspy_connection_list_names_finish (DspyConnection  *self,
                                   GAsyncResult    *result,
                                   GError         **error)
{
  g_return_val_if_fail (DSPY_IS_CONNECTION (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * dspy_connection_get_has_error:
 *
 * Checks if any errors have been registered with the connection, such
 * as when listing peer names.
 *
 * This can be used to show extra information to the user about the
 * connection issues.
 *
 * Returns: %TRUE if there are any errors
 */
gboolean
dspy_connection_get_has_error (DspyConnection *self)
{
  g_return_val_if_fail (DSPY_IS_CONNECTION (self), FALSE);

  return self->errors != NULL && self->errors->len > 0;
}

void
dspy_connection_add_error (DspyConnection *self,
                           const GError   *error)
{
  gboolean notify;

  g_return_if_fail (DSPY_IS_CONNECTION (self));
  g_return_if_fail (error != NULL);

  if (self->errors == NULL)
    self->errors = g_ptr_array_new_with_free_func ((GDestroyNotify)g_error_free);

  notify = self->errors->len == 0;

  g_ptr_array_add (self->errors, g_error_copy (error));

  g_signal_emit (self, signals [ERROR], 0, error);

  if (notify)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_ERROR]);
}

void
dspy_connection_clear_errors (DspyConnection *self)
{
  g_return_if_fail (DSPY_IS_CONNECTION (self));

  if (self->errors != NULL && self->errors->len > 0)
    {
      g_ptr_array_remove_range (self->errors, 0, self->errors->len);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_ERROR]);
    }
}
