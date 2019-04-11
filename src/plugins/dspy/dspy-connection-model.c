/* dspy-connection-model.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "dspy-connection-model"

#include "config.h"

#include "dspy-connection-model.h"
#include "dspy-name.h"

struct _DspyConnectionModel
{
  GObject          parent;

  GDBusConnection *connection;
  GCancellable    *cancellable;
  GSequence       *peers;
  GDBusProxy      *bus_proxy;

  guint            name_owner_changed_handler;
};

enum {
  PROP_0,
  PROP_CONNECTION,
  N_PROPS
};

static guint
dspy_connection_model_get_n_items (GListModel *model)
{
  DspyConnectionModel *self = DSPY_CONNECTION_MODEL (model);
  return g_sequence_get_length (self->peers);
}

static GType
dspy_connection_model_get_item_type (GListModel *model)
{
  /* XXX: switch to type */
  return G_TYPE_OBJECT;
}

static gpointer
dspy_connection_model_get_item (GListModel *model,
                                guint       position)
{
  DspyConnectionModel *self = DSPY_CONNECTION_MODEL (model);
  GSequenceIter *iter = g_sequence_get_iter_at_pos (self->peers, position);

  if (g_sequence_iter_is_end (iter))
    return NULL;

  return g_object_ref (g_sequence_get (iter));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = dspy_connection_model_get_item_type;
  iface->get_n_items = dspy_connection_model_get_n_items;
  iface->get_item = dspy_connection_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (DspyConnectionModel, dspy_connection_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
dspy_connection_model_finalize (GObject *object)
{
  DspyConnectionModel *self = (DspyConnectionModel *)object;

  g_assert (self->connection == NULL);
  g_assert (self->cancellable == NULL);

  g_clear_object (&self->connection);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->peers, g_sequence_free);

  G_OBJECT_CLASS (dspy_connection_model_parent_class)->finalize (object);
}

static void
dspy_connection_model_dispose (GObject *object)
{
  DspyConnectionModel *self = (DspyConnectionModel *)object;

  dspy_connection_model_set_connection (self, NULL);

  G_OBJECT_CLASS (dspy_connection_model_parent_class)->dispose (object);
}

static void
dspy_connection_model_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  DspyConnectionModel *self = DSPY_CONNECTION_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, dspy_connection_model_get_connection (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_connection_model_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  DspyConnectionModel *self = DSPY_CONNECTION_MODEL (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      dspy_connection_model_set_connection (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_connection_model_class_init (DspyConnectionModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = dspy_connection_model_dispose;
  object_class->finalize = dspy_connection_model_finalize;
  object_class->get_property = dspy_connection_model_get_property;
  object_class->set_property = dspy_connection_model_set_property;

  /**
   * DspyConnectionModel:connection:
   *
   * The "connection" property contains the #GDBusConnection that will be monitored
   * for changes to the bus.
   */
  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection",
                         "Connection",
                         "A GDBus connection for the source of bus changes",
                         G_TYPE_DBUS_CONNECTION,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
dspy_connection_model_init (DspyConnectionModel *self)
{
  self->peers = g_sequence_new (g_object_unref);
}

static void
dspy_connection_model_name_owner_changed_cb (GDBusConnection *connection,
                                             const gchar     *sender_name,
                                             const gchar     *object_path,
                                             const gchar     *interface_name,
                                             const gchar     *signal_name,
                                             GVariant        *params,
                                             gpointer         user_data)
{
  DspyConnectionModel *self = user_data;

  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (DSPY_IS_CONNECTION_MODEL (self));

}

static void
dspy_connection_model_get_process_id_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GDBusProxy *proxy = (GDBusProxy *)object;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(DspyName) name = user_data;
  g_autoptr(GError) error = NULL;
  GPid pid = 0;

  g_assert (DSPY_IS_NAME (name));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DSPY_IS_NAME (name));

  if ((ret = g_dbus_proxy_call_finish (proxy, result, &error)))
    g_variant_get (ret, "(u)", &pid);

  dspy_name_set_pid (name, pid);
}

static void
dspy_connection_model_add_names (DspyConnectionModel *self,
                                 GVariant            *names,
                                 gboolean             activatable)
{
  const gchar * const *strv = NULL;
  GVariantIter iter;

  g_assert (DSPY_IS_CONNECTION_MODEL (self));
  g_assert (names != NULL);

  g_variant_iter_init (&iter, names);
  if (g_variant_iter_next (&iter, "^as", &strv))
    {
      for (guint i = 0; strv[i]; i++)
        {
          g_autoptr(DspyName) name = dspy_name_new (strv[i], activatable);
          GSequenceIter *seq;
          guint removed = 0;

          /* Skip if we already know about the name, but replace
           * the item if we found something activatable.
           */
          if ((seq = g_sequence_lookup (self->peers,
                                        name,
                                        (GCompareDataFunc) dspy_name_compare,
                                        NULL)))
            {
              if (!activatable)
                continue;
              g_sequence_remove (seq);
              removed++;
            }

          seq = g_sequence_insert_sorted (self->peers,
                                          g_object_ref (name),
                                          (GCompareDataFunc) dspy_name_compare,
                                          NULL);

          g_list_model_items_changed (G_LIST_MODEL (self),
                                      g_sequence_iter_get_position (seq),
                                      removed, 1);

          g_dbus_proxy_call (self->bus_proxy,
                             "GetConnectionUnixProcessID",
                             g_variant_new ("(s)", strv[i]),
                             G_DBUS_CALL_FLAGS_NONE,
                             -1,
                             self->cancellable,
                             dspy_connection_model_get_process_id_cb,
                             g_object_ref (name));
        }
    }
}

static void
dspy_connection_model_list_activatable_names_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  GDBusProxy *proxy = (GDBusProxy *)object;
  g_autoptr(DspyConnectionModel) self = user_data;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_DBUS_PROXY (proxy));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DSPY_IS_CONNECTION_MODEL (self));

  if (!(ret = g_dbus_proxy_call_finish (proxy, result, &error)))
    g_warning ("Failed to list activatable names: %s", error->message);
  else
    dspy_connection_model_add_names (self, ret, TRUE);
}

static void
dspy_connection_model_list_names_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GDBusProxy *proxy = (GDBusProxy *)object;
  g_autoptr(DspyConnectionModel) self = user_data;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_DBUS_PROXY (proxy));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DSPY_IS_CONNECTION_MODEL (self));

  if (!(ret = g_dbus_proxy_call_finish (proxy, result, &error)))
    g_warning ("Failed to list names: %s", error->message);
  else
    dspy_connection_model_add_names (self, ret, FALSE);
}

DspyConnectionModel *
dspy_connection_model_new (void)
{
  return g_object_new (DSPY_TYPE_CONNECTION_MODEL, NULL);
}

/**
 * dspy_connection_model_get_connection:
 * @self: a #DspyConnectionModel
 *
 * Gets the #GDBusConnection used for the model.
 *
 * Returns: (transfer none) (nullable): a #GDBusConnection or %NULL
 */
GDBusConnection *
dspy_connection_model_get_connection (DspyConnectionModel *self)
{
  g_return_val_if_fail (DSPY_IS_CONNECTION_MODEL (self), NULL);

  return self->connection;
}

void
dspy_connection_model_set_connection (DspyConnectionModel *self,
                                      GDBusConnection     *connection)
{
  g_return_if_fail (DSPY_IS_CONNECTION_MODEL (self));

  if (self->connection == connection)
    return;

  if (self->connection != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->name_owner_changed_handler);
      g_clear_object (&self->cancellable);
      g_clear_object (&self->connection);
      g_clear_object (&self->bus_proxy);
      g_clear_pointer (&self->peers, g_sequence_free);
      self->peers = g_sequence_new (g_object_unref);
    }

  g_assert (self->cancellable == NULL);
  g_assert (self->connection == NULL);
  g_assert (self->bus_proxy == NULL);
  g_assert (g_sequence_is_empty (self->peers));

  if (connection != NULL)
    {
      g_autoptr(GError) error = NULL;

      self->connection = g_object_ref (connection);
      self->cancellable = g_cancellable_new ();
      self->name_owner_changed_handler =
        g_dbus_connection_signal_subscribe (self->connection,
                                            NULL,
                                            "org.freedesktop.DBus",
                                            "NameOwnerChanged",
                                            NULL,
                                            NULL,
                                            0,
                                            dspy_connection_model_name_owner_changed_cb,
                                            self,
                                            NULL);
      self->bus_proxy = g_dbus_proxy_new_sync (self->connection,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               NULL,
                                               "org.freedesktop.DBus",
                                               "/org/freedesktop/DBus",
                                               "org.freedesktop.DBus",
                                               self->cancellable, &error);

      if (self->bus_proxy == NULL)
        {
          g_warning ("Failed to create DBus proxy: %s", error->message);
          goto notify;
        }

      g_dbus_proxy_call (self->bus_proxy,
                         "ListActivatableNames",
                         g_variant_new ("()"),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         self->cancellable,
                         dspy_connection_model_list_activatable_names_cb,
                         g_object_ref (self));

      g_dbus_proxy_call (self->bus_proxy,
                         "ListNames",
                         g_variant_new ("()"),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         self->cancellable,
                         dspy_connection_model_list_names_cb,
                         g_object_ref (self));
    }

notify:
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONNECTION]);
}
