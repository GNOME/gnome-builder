/* dspy-method-invocation.c
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

#define G_LOG_DOMAIN "dspy-method-invocation"

#include "config.h"

#include "dspy-method-invocation.h"

typedef struct
{
  gchar    *interface;
  gchar    *signature;
  gchar    *object_path;
  gchar    *method;
  gchar    *reply_signature;
  DspyName *name;
  GVariant *parameters;
  gint      timeout_msec;
} DspyMethodInvocationPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (DspyMethodInvocation, dspy_method_invocation, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_INTERFACE,
  PROP_METHOD,
  PROP_NAME,
  PROP_OBJECT_PATH,
  PROP_PARAMETERS,
  PROP_REPLY_SIGNATURE,
  PROP_SIGNATURE,
  PROP_TIMEOUT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * dspy_method_invocation_new:
 *
 * Create a new #DspyMethodInvocation.
 *
 * Returns: (transfer full): a newly created #DspyMethodInvocation
 */
DspyMethodInvocation *
dspy_method_invocation_new (void)
{
  return g_object_new (DSPY_TYPE_METHOD_INVOCATION, NULL);
}

static void
dspy_method_invocation_finalize (GObject *object)
{
  DspyMethodInvocation *self = (DspyMethodInvocation *)object;
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_clear_pointer (&priv->interface, g_free);
  g_clear_pointer (&priv->signature, g_free);
  g_clear_pointer (&priv->object_path, g_free);
  g_clear_pointer (&priv->method, g_free);
  g_clear_pointer (&priv->reply_signature, g_free);
  g_clear_object (&priv->name);
  g_clear_pointer (&priv->parameters, g_variant_unref);

  G_OBJECT_CLASS (dspy_method_invocation_parent_class)->finalize (object);
}

static void
dspy_method_invocation_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  DspyMethodInvocation *self = DSPY_METHOD_INVOCATION (object);

  switch (prop_id)
    {
    case PROP_INTERFACE:
      g_value_set_string (value, dspy_method_invocation_get_interface (self));
      break;

    case PROP_OBJECT_PATH:
      g_value_set_string (value, dspy_method_invocation_get_object_path (self));
      break;

    case PROP_METHOD:
      g_value_set_string (value, dspy_method_invocation_get_method (self));
      break;

    case PROP_SIGNATURE:
      g_value_set_string (value, dspy_method_invocation_get_signature (self));
      break;

    case PROP_REPLY_SIGNATURE:
      g_value_set_string (value, dspy_method_invocation_get_reply_signature (self));
      break;

    case PROP_NAME:
      g_value_set_object (value, dspy_method_invocation_get_name (self));
      break;

    case PROP_PARAMETERS:
      g_value_set_variant (value, dspy_method_invocation_get_parameters (self));
      break;

    case PROP_TIMEOUT:
      g_value_set_int (value, dspy_method_invocation_get_timeout (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_method_invocation_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  DspyMethodInvocation *self = DSPY_METHOD_INVOCATION (object);

  switch (prop_id)
    {
    case PROP_INTERFACE:
      dspy_method_invocation_set_interface (self, g_value_get_string (value));
      break;

    case PROP_OBJECT_PATH:
      dspy_method_invocation_set_object_path (self, g_value_get_string (value));
      break;

    case PROP_METHOD:
      dspy_method_invocation_set_method (self, g_value_get_string (value));
      break;

    case PROP_SIGNATURE:
      dspy_method_invocation_set_signature (self, g_value_get_string (value));
      break;

    case PROP_REPLY_SIGNATURE:
      dspy_method_invocation_set_reply_signature (self, g_value_get_string (value));
      break;

    case PROP_NAME:
      dspy_method_invocation_set_name (self, g_value_get_object (value));
      break;

    case PROP_PARAMETERS:
      dspy_method_invocation_set_parameters (self, g_value_get_variant (value));
      break;

    case PROP_TIMEOUT:
      dspy_method_invocation_set_timeout (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_method_invocation_class_init (DspyMethodInvocationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = dspy_method_invocation_finalize;
  object_class->get_property = dspy_method_invocation_get_property;
  object_class->set_property = dspy_method_invocation_set_property;

  properties [PROP_INTERFACE] =
    g_param_spec_string ("interface",
                         "Interface",
                         "The interface containing the method",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_OBJECT_PATH] =
    g_param_spec_string ("object-path",
                         "Object Path",
                         "The path containing the interface",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_METHOD] =
    g_param_spec_string ("method",
                         "Method",
                         "The method of the interface to execute",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SIGNATURE] =
    g_param_spec_string ("signature",
                         "Signature",
                         "The signature of the method, used for display purposes",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_REPLY_SIGNATURE] =
    g_param_spec_string ("reply-signature",
                         "Reply Signature",
                         "The reply signature of the method, used for display purposes",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_object ("name",
                         "Name",
                         "The DspyName to communicate with",
                         DSPY_TYPE_NAME,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PARAMETERS] =
    g_param_spec_variant ("parameters",
                          "Parameters",
                          "The parameters for the invocation",
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TIMEOUT] =
    g_param_spec_int ("timeout",
                      "Timeout",
                      "The timeout for the operation",
                      -1, G_MAXINT, -1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
dspy_method_invocation_init (DspyMethodInvocation *self)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  priv->timeout_msec = -1;
}

static void
dspy_method_invocation_execute_call_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GDBusConnection *bus = (GDBusConnection *)object;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_DBUS_CONNECTION (bus));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!(reply = g_dbus_connection_call_finish (bus, result, &error)))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&reply), (GDestroyNotify)g_variant_unref);
}

static void
dspy_method_invocation_execute_open_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  DspyMethodInvocationPrivate *priv;
  DspyMethodInvocation *self;
  DspyConnection *connection = (DspyConnection *)object;
  g_autoptr(GDBusConnection) bus = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;

  g_assert (DSPY_IS_CONNECTION (connection));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!(bus = dspy_connection_open_finish (connection, result, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = g_task_get_source_object (task);
  priv = dspy_method_invocation_get_instance_private (self);
  cancellable = g_task_get_cancellable (task);

  if (priv->name == NULL ||
      priv->object_path == NULL ||
      priv->interface == NULL ||
      priv->method == NULL ||
      priv->parameters == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_INITIALIZED,
                               "Method invocation contains uninitialized parameters");
      return;
    }

  g_dbus_connection_call (bus,
                          dspy_name_get_owner (priv->name),
                          priv->object_path,
                          priv->interface,
                          priv->method,
                          priv->parameters,
                          NULL, /* Allow any reply type (even if invalid) */
                          G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
                          priv->timeout_msec,
                          cancellable,
                          dspy_method_invocation_execute_call_cb,
                          g_steal_pointer (&task));
}

void
dspy_method_invocation_execute_async (DspyMethodInvocation *self,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  DspyConnection *connection;

  g_assert (DSPY_IS_METHOD_INVOCATION (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, dspy_method_invocation_execute_async);

  if (priv->name == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_INITIALIZED,
                               "No name set to communicate with");
      return;
    }

  connection = dspy_name_get_connection (priv->name);

  dspy_connection_open_async (connection,
                              cancellable,
                              dspy_method_invocation_execute_open_cb,
                              g_steal_pointer (&task));
}

/**
 * dspy_method_invocation_execute_finish:
 *
 * Completes an asynchronous call to dspy_method_invocation_execute_async()
 *
 * Returns: (transfer full): a #GVariant if successful; otherwise %FALSE and
 *   @error is set.
 */
GVariant *
dspy_method_invocation_execute_finish (DspyMethodInvocation  *self,
                                       GAsyncResult          *result,
                                       GError               **error)
{
  g_return_val_if_fail (DSPY_IS_METHOD_INVOCATION (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

const gchar *
dspy_method_invocation_get_interface (DspyMethodInvocation *self)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);
  g_return_val_if_fail (DSPY_IS_METHOD_INVOCATION (self), NULL);
  return priv->interface;
}

const gchar *
dspy_method_invocation_get_object_path (DspyMethodInvocation *self)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);
  g_return_val_if_fail (DSPY_IS_METHOD_INVOCATION (self), NULL);
  return priv->object_path;
}

const gchar *
dspy_method_invocation_get_method (DspyMethodInvocation *self)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);
  g_return_val_if_fail (DSPY_IS_METHOD_INVOCATION (self), NULL);
  return priv->method;
}

const gchar *
dspy_method_invocation_get_signature (DspyMethodInvocation *self)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);
  g_return_val_if_fail (DSPY_IS_METHOD_INVOCATION (self), NULL);
  return priv->signature;
}

const gchar *
dspy_method_invocation_get_reply_signature (DspyMethodInvocation *self)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);
  g_return_val_if_fail (DSPY_IS_METHOD_INVOCATION (self), NULL);
  return priv->reply_signature;
}

/**
 * dspy_method_invocation_get_parameters:
 *
 * Returns: (transfer none): a #GVariant if set; otherwise %NULL
 */
GVariant *
dspy_method_invocation_get_parameters (DspyMethodInvocation *self)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_val_if_fail (DSPY_IS_METHOD_INVOCATION (self), NULL);

  return priv->parameters;
}

/**
 * dspy_method_invocation_get_name:
 *
 * Returns: (transfer none) (nullable): a #DspyName or %NULL if unset
 */
DspyName *
dspy_method_invocation_get_name (DspyMethodInvocation *self)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_val_if_fail (DSPY_IS_METHOD_INVOCATION (self), NULL);

  return priv->name;
}

void
dspy_method_invocation_set_interface (DspyMethodInvocation *self,
                                      const gchar          *interface)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_if_fail (DSPY_IS_METHOD_INVOCATION (self));

  if (g_strcmp0 (priv->interface, interface) != 0)
    {
      g_free (priv->interface);
      priv->interface = g_strdup (interface);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INTERFACE]);
    }
}

void
dspy_method_invocation_set_method (DspyMethodInvocation *self,
                                   const gchar          *method)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_if_fail (DSPY_IS_METHOD_INVOCATION (self));

  if (g_strcmp0 (priv->method, method) != 0)
    {
      g_free (priv->method);
      priv->method = g_strdup (method);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_METHOD]);
    }
}

void
dspy_method_invocation_set_object_path (DspyMethodInvocation *self,
                                        const gchar          *object_path)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_if_fail (DSPY_IS_METHOD_INVOCATION (self));

  if (g_strcmp0 (priv->object_path, object_path) != 0)
    {
      g_free (priv->object_path);
      priv->object_path = g_strdup (object_path);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_OBJECT_PATH]);
    }
}

void
dspy_method_invocation_set_signature (DspyMethodInvocation *self,
                                      const gchar          *signature)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_if_fail (DSPY_IS_METHOD_INVOCATION (self));

  if (g_strcmp0 (priv->signature, signature) != 0)
    {
      g_free (priv->signature);
      priv->signature = g_strdup (signature);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SIGNATURE]);
    }
}

void
dspy_method_invocation_set_reply_signature (DspyMethodInvocation *self,
                                            const gchar          *reply_signature)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_if_fail (DSPY_IS_METHOD_INVOCATION (self));

  if (g_strcmp0 (priv->reply_signature, reply_signature) != 0)
    {
      g_free (priv->reply_signature);
      priv->reply_signature = g_strdup (reply_signature);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REPLY_SIGNATURE]);
    }
}

void
dspy_method_invocation_set_name (DspyMethodInvocation *self,
                                 DspyName             *name)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_if_fail (DSPY_IS_METHOD_INVOCATION (self));

  if (g_set_object (&priv->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
}

void
dspy_method_invocation_set_parameters (DspyMethodInvocation *self,
                                       GVariant             *parameters)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_if_fail (DSPY_IS_METHOD_INVOCATION (self));

  if (parameters != priv->parameters)
    {
      g_clear_pointer (&priv->parameters, g_variant_unref);
      priv->parameters = parameters ? g_variant_ref_sink (parameters) : NULL;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PARAMETERS]);
    }
}

gint
dspy_method_invocation_get_timeout (DspyMethodInvocation *self)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_val_if_fail (DSPY_IS_METHOD_INVOCATION (self), -1);

  return priv->timeout_msec;
}

void
dspy_method_invocation_set_timeout (DspyMethodInvocation *self,
                                    gint                  timeout)
{
  DspyMethodInvocationPrivate *priv = dspy_method_invocation_get_instance_private (self);

  g_return_if_fail (DSPY_IS_METHOD_INVOCATION (self));
  g_return_if_fail (timeout >= -1);

  if (priv->timeout_msec != timeout)
    {
      priv->timeout_msec = timeout;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TIMEOUT]);
    }
}
