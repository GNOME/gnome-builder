/* ide-gca-service.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "ide-gca-service"

#include <glib/gi18n.h>

#include "ide-gca-service.h"
#include "ide-macros.h"

struct _IdeGcaService
{
  IdeObject        parent_instance;

  GDBusConnection *bus;
  GHashTable      *proxy_cache;

  gulong           bus_closed_handler;
};

G_DEFINE_TYPE_EXTENDED (IdeGcaService, ide_gca_service, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, NULL))

static void
on_bus_closed (GDBusConnection *bus,
               gboolean         remote_peer_vanished,
               GError          *error,
               gpointer         user_data)
{
  IdeGcaService *self = user_data;

  g_assert (G_IS_DBUS_CONNECTION (bus));
  g_assert (IDE_IS_GCA_SERVICE (self));

  if (self->bus_closed_handler != 0)
    ide_clear_signal_handler (bus, &self->bus_closed_handler);

  g_clear_object (&self->bus);
  g_hash_table_remove_all (self->proxy_cache);
}

static GDBusConnection *
ide_gca_service_get_bus (IdeGcaService  *self,
                         GCancellable   *cancellable,
                         GError        **error)
{
  g_assert (IDE_IS_GCA_SERVICE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->bus == NULL)
    {
      const GDBusConnectionFlags flags = (G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                          G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION);
      g_autofree gchar *address = NULL;
      g_autoptr(GDBusConnection) bus = NULL;

      address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, cancellable, error);
      if (address == NULL)
        return NULL;

      bus = g_dbus_connection_new_for_address_sync (address, flags, NULL, cancellable, error);
      if (bus == NULL)
        return NULL;

      self->bus_closed_handler = g_signal_connect (bus,
                                                   "closed",
                                                   G_CALLBACK (on_bus_closed),
                                                   self);

      g_dbus_connection_set_exit_on_close (bus, FALSE);

      self->bus = g_object_ref (bus);
    }

  return self->bus;
}

static const gchar *
remap_language (const gchar *lang_id)
{
  static GHashTable *remap;
  gchar *remapped_lang_id;

  if (lang_id == NULL)
    return NULL;

  if (remap == NULL)
    {
      remap = g_hash_table_new (g_str_hash, g_str_equal);
#define ADD_REMAP(key,val) g_hash_table_insert (remap, (gchar *)key, (gchar *)val)
      ADD_REMAP ("chdr", "c");
      ADD_REMAP ("cpp", "c");
      ADD_REMAP ("objc", "c");
      ADD_REMAP ("scss", "css");
#undef ADD_REMAP
    }

  remapped_lang_id = g_hash_table_lookup (remap, lang_id);

  if (remapped_lang_id == NULL)
    return lang_id;
  else
    return remapped_lang_id;
}

static void
proxy_new_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  IdeGcaService *self;
  g_autoptr(GTask) task = user_data;
  const gchar *language_id;
  GcaService *proxy;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  self = g_task_get_source_object (task);

  proxy = gca_service_proxy_new_finish (result, &error);

  if (!proxy)
    {
      g_task_return_error (task, error);
      return;
    }

  language_id = g_task_get_task_data (task);
  g_hash_table_replace (self->proxy_cache, g_strdup (language_id),
                        g_object_ref (proxy));

  g_task_return_pointer (task, g_object_ref (proxy), g_object_unref);

  g_clear_object (&proxy);
}

void
ide_gca_service_get_proxy_async (IdeGcaService       *self,
                                 const gchar         *language_id,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *object_path = NULL;
  GcaService *proxy;
  GDBusConnection *bus;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_GCA_SERVICE (self));
  g_return_if_fail (language_id);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  language_id = remap_language (language_id);

  if (!language_id)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("No language specified"));
      return;
    }

  bus = ide_gca_service_get_bus (self, cancellable, &error);

  if (bus == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  if ((proxy = g_hash_table_lookup (self->proxy_cache, language_id)))
    {
      g_task_return_pointer (task, g_object_ref (proxy), g_object_unref);
      return;
    }

  g_task_set_task_data (task, g_strdup (language_id), g_free);

  name = g_strdup_printf ("org.gnome.CodeAssist.v1.%s", language_id);
  object_path = g_strdup_printf ("/org/gnome/CodeAssist/v1/%s", language_id);

  gca_service_proxy_new (bus,
                         G_DBUS_PROXY_FLAGS_NONE,
                         name,
                         object_path,
                         cancellable,
                         proxy_new_cb,
                         g_object_ref (task));
}

/**
 * ide_gca_service_get_proxy_finish:
 *
 * Completes an asynchronous request to load a Gca proxy.
 *
 * Returns: (transfer full): A #GcaService or %NULL upon failure.
 */
GcaService *
ide_gca_service_get_proxy_finish (IdeGcaService  *self,
                                  GAsyncResult   *result,
                                  GError        **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_GCA_SERVICE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_gca_service_finalize (GObject *object)
{
  IdeGcaService *self = (IdeGcaService *)object;

  if (self->bus != NULL)
    {
      ide_clear_signal_handler (self->bus, &self->bus_closed_handler);
      g_clear_object (&self->bus);
    }

  g_clear_pointer (&self->proxy_cache, g_hash_table_unref);

  G_OBJECT_CLASS (ide_gca_service_parent_class)->finalize (object);
}

static void
ide_gca_service_class_init (IdeGcaServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_gca_service_finalize;
}

static void
ide_gca_service_init (IdeGcaService *self)
{
  self->proxy_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, g_object_unref);
}
