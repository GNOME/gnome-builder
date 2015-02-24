/* ide-gca-service.c
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

#include <gca-service.h>
#include <glib/gi18n.h>

#include "ide-gca-service.h"

struct _IdeGcaService
{
  IdeService  parent_instance;

  GHashTable *proxy_cache;
};

static GDBusConnection *gDBus;

G_DEFINE_TYPE (IdeGcaService, ide_gca_service, IDE_TYPE_SERVICE)

static const gchar *
remap_language (const gchar *lang_id)
{
  g_return_val_if_fail (lang_id, NULL);

  if (g_str_equal (lang_id, "chdr") ||
      g_str_equal (lang_id, "objc") ||
      g_str_equal (lang_id, "cpp"))
    return "c";

  return lang_id;
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

  g_return_if_fail (IDE_IS_GCA_SERVICE (self));
  g_return_if_fail (language_id);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  language_id = remap_language (language_id);

  task = g_task_new (self, cancellable, callback, user_data);

  if (!gDBus)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               _("Not connected to DBus."));
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

  gca_service_proxy_new (gDBus,
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

  g_clear_pointer (&self->proxy_cache, g_hash_table_unref);

  G_OBJECT_CLASS (ide_gca_service_parent_class)->finalize (object);
}

static void
ide_gca_service_class_init (IdeGcaServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GError *error = NULL;

  object_class->finalize = ide_gca_service_finalize;

  gDBus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  if (!gDBus)
    {
      g_warning (_("Failed to load DBus connection to session bus. "
                   "Code assistance will be disabled. "
                   "Error was: %s"),
                 error->message);
      g_clear_error (&error);
    }
}

static void
ide_gca_service_init (IdeGcaService *self)
{
  self->proxy_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, g_object_unref);
}
