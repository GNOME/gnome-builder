/* dspy-name.c
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

#define G_LOG_DOMAIN "dspy-name"

#include "config.h"

#include "dspy-introspection-model.h"
#include "dspy-name.h"
#include "dspy-private.h"

struct _DspyName
{
  GObject         parent_instance;
  DspyConnection *connection;
  gchar          *name;
  gchar          *owner;
  gchar          *search_text;
  GPid            pid;
  guint           activatable : 1;
};

enum {
  PROP_0,
  PROP_ACTIVATABLE,
  PROP_CONNECTION,
  PROP_NAME,
  PROP_OWNER,
  PROP_PID,
  N_PROPS
};

G_DEFINE_TYPE (DspyName, dspy_name, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
dspy_name_finalize (GObject *object)
{
  DspyName *self = (DspyName *)object;

  g_clear_object (&self->connection);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->owner, g_free);
  g_clear_pointer (&self->search_text, g_free);

  G_OBJECT_CLASS (dspy_name_parent_class)->finalize (object);
}

static void
dspy_name_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  DspyName *self = DSPY_NAME (object);

  switch (prop_id)
    {
    case PROP_ACTIVATABLE:
      g_value_set_boolean (value, dspy_name_get_activatable (self));
      break;

    case PROP_CONNECTION:
      g_value_set_object (value, dspy_name_get_connection (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, dspy_name_get_name (self));
      break;

    case PROP_OWNER:
      g_value_set_string (value, dspy_name_get_owner (self));
      break;

    case PROP_PID:
      g_value_set_int (value, dspy_name_get_pid (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_name_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  DspyName *self = DSPY_NAME (object);

  switch (prop_id)
    {
    case PROP_ACTIVATABLE:
      self->activatable = g_value_get_boolean (value);
      break;

    case PROP_CONNECTION:
      self->connection = g_value_dup_object (value);
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_name_class_init (DspyNameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = dspy_name_finalize;
  object_class->get_property = dspy_name_get_property;
  object_class->set_property = dspy_name_set_property;

  properties [PROP_ACTIVATABLE] =
    g_param_spec_boolean ("activatable",
                          "Activatable",
                          "Activatable",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection",
                         "Connection",
                         "The connection where the name can be found",
                         DSPY_TYPE_CONNECTION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The peer name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_OWNER] =
    g_param_spec_string ("owner",
                         "Owner",
                         "The owner of the D-Bus name",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PID] =
    g_param_spec_int ("pid",
                      "Pid",
                      "The pid of the peer",
                      -1, G_MAXINT, -1,
                      (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
dspy_name_init (DspyName *self)
{
  self->pid = -1;
}

DspyName *
dspy_name_new (DspyConnection *connection,
               const gchar    *name,
               gboolean        activatable)
{
  return g_object_new (DSPY_TYPE_NAME,
                       "activatable", activatable,
                       "connection", connection,
                       "name", name,
                       NULL);
}

gboolean
dspy_name_get_activatable (DspyName *self)
{
  g_return_val_if_fail (DSPY_IS_NAME (self), FALSE);

  return self->activatable;
}

void
_dspy_name_set_activatable (DspyName *self,
                            gboolean  activatable)
{
  g_return_if_fail (DSPY_IS_NAME (self));

  activatable = !!activatable;

  if (self->activatable != activatable)
    {
      self->activatable = activatable;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVATABLE]);
    }
}

const gchar *
dspy_name_get_name (DspyName *self)
{
  g_return_val_if_fail (DSPY_IS_NAME (self), NULL);

  return self->name;
}

gint
dspy_name_compare (gconstpointer a,
                   gconstpointer b)
{
  DspyName *item1 = DSPY_NAME ((gpointer)a);
  DspyName *item2 = DSPY_NAME ((gpointer)b);
  const gchar *name1 = dspy_name_get_name (item1);
  const gchar *name2 = dspy_name_get_name (item2);

  if (name1[0] != name2[0])
    {
      if (name1[0] == ':')
        return 1;
      if (name2[0] == ':')
        return -1;
    }

  /* Sort numbers like :1.300 better */
  if (g_str_has_prefix (name1, ":1.") &&
      g_str_has_prefix (name2, ":1."))
    {
      gint i1 = g_ascii_strtoll (name1 + 3, NULL, 10);
      gint i2 = g_ascii_strtoll (name2 + 3, NULL, 10);

      return i1 - i2;
    }

  return g_strcmp0 (name1, name2);
}

GPid
dspy_name_get_pid (DspyName *self)
{
  g_return_val_if_fail (DSPY_IS_NAME (self), 0);

  return self->pid;
}

const gchar *
dspy_name_get_owner (DspyName *self)
{
  g_return_val_if_fail (DSPY_IS_NAME (self), NULL);

  return self->owner ? self->owner : self->name;
}

void
_dspy_name_set_owner (DspyName    *self,
                      const gchar *owner)
{
  g_return_if_fail (DSPY_IS_NAME (self));

  if (g_strcmp0 (owner, self->owner) != 0)
    {
      g_free (self->owner);
      self->owner = g_strdup (owner);
      g_clear_pointer (&self->search_text, g_free);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_OWNER]);
    }
}

void
_dspy_name_clear_pid (DspyName *self)
{
  g_return_if_fail (DSPY_IS_NAME (self));

  if (self->pid != -1)
    {
      self->pid = -1;
      g_clear_pointer (&self->search_text, g_free);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PID]);
    }
}

static void
dspy_name_get_pid_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GDBusConnection *connection = (GDBusConnection *)object;
  g_autoptr(DspyName) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  guint pid;

  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DSPY_IS_NAME (self));

  if (!(reply = g_dbus_connection_call_finish (connection, result, &error)))
    return;

  g_variant_get (reply, "(u)", &pid);

  if (self->pid != pid)
    {
      self->pid = pid;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PID]);
    }
}

void
_dspy_name_refresh_pid (DspyName        *self,
                        GDBusConnection *connection)
{
  g_return_if_fail (DSPY_IS_NAME (self));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  g_dbus_connection_call (connection,
                          "org.freedesktop.DBus",
                          "/org/freedesktop/DBus",
                          "org.freedesktop.DBus",
                          "GetConnectionUnixProcessID",
                          g_variant_new ("(s)", self->name),
                          G_VARIANT_TYPE ("(u)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          dspy_name_get_pid_cb,
                          g_object_ref (self));
}

static void
dspy_name_get_owner_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GDBusConnection *connection = (GDBusConnection *)object;
  g_autoptr(DspyName) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  const gchar *owner = NULL;

  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DSPY_IS_NAME (self));

  if (!(reply = g_dbus_connection_call_finish (connection, result, &error)))
    return;

  g_variant_get (reply, "(&s)", &owner);
  _dspy_name_set_owner (self, owner);
}

void
_dspy_name_refresh_owner (DspyName        *self,
                          GDBusConnection *connection)
{
  g_return_if_fail (DSPY_IS_NAME (self));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  g_clear_pointer (&self->owner, g_free);

  /* If the name is already a :0.123 style name, that's the owner */
  if (self->name[0] == ':')
    return;

  g_dbus_connection_call (connection,
                          "org.freedesktop.DBus",
                          "/org/freedesktop/DBus",
                          "org.freedesktop.DBus",
                          "GetNameOwner",
                          g_variant_new ("(s)", self->name),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          dspy_name_get_owner_cb,
                          g_object_ref (self));
}

/**
 * dspy_name_get_connection:
 *
 * Gets the connection that is to be used.
 *
 * Returns: (transer none): a #DspyConnection or %NULL
 */
DspyConnection *
dspy_name_get_connection (DspyName *self)
{
  g_return_val_if_fail (DSPY_IS_NAME (self), NULL);

  return self->connection;
}

static void
dspy_name_introspection_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  g_assert (G_IS_ASYNC_INITABLE (initable));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!g_async_initable_init_finish (initable, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_object_ref (initable), g_object_unref);
}

void
dspy_name_introspect_async (DspyName            *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(DspyIntrospectionModel) model = NULL;

  g_return_if_fail (DSPY_IS_NAME (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, dspy_name_introspect_async);

  model = _dspy_introspection_model_new (self);

  g_async_initable_init_async (G_ASYNC_INITABLE (model),
                               G_PRIORITY_DEFAULT,
                               cancellable,
                               dspy_name_introspection_cb,
                               g_steal_pointer (&task));

}

/**
 * dspy_name_introspect_finish:
 *
 * Completes a request to dspy_name_introspect_async().
 *
 * Returns: (transfer full): a #GtkTreeModel if successful; otherwise
 *   %NULL and @error is set.
 */
GtkTreeModel *
dspy_name_introspect_finish (DspyName      *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (DSPY_IS_NAME (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

const gchar *
dspy_name_get_search_text (DspyName *self)
{
  g_return_val_if_fail (DSPY_IS_NAME (self), FALSE);

  if (self->search_text == NULL)
    {
      const gchar *owner = dspy_name_get_owner (self);
      self->search_text = g_strdup_printf ("%s %s %d", self->name, owner, self->pid);
    }

  return self->search_text;
}
