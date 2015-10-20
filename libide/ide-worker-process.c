/* ide-worker-process.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-worker-process"

#include <libpeas/peas.h>

#include "egg-counter.h"

#include "ide-debug.h"
#include "ide-worker-process.h"
#include "ide-worker.h"

struct _IdeWorkerProcess
{
  GObject          parent_instance;

  gchar           *argv0;
  gchar           *dbus_address;
  gchar           *plugin_name;
  GSubprocess     *subprocess;
  GDBusConnection *connection;

  guint            quit : 1;
};

static void ide_worker_process_respawn (IdeWorkerProcess *self);

G_DEFINE_TYPE (IdeWorkerProcess, ide_worker_process, G_TYPE_OBJECT)

EGG_DEFINE_COUNTER (instances, "IdeWorkerProcess", "Instances", "Number of IdeWorkerProcess instances")

enum {
  PROP_0,
  PROP_ARGV0,
  PROP_PLUGIN_NAME,
  PROP_DBUS_ADDRESS,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

IdeWorkerProcess *
ide_worker_process_new (const gchar *argv0,
                        const gchar *plugin_name,
                        const gchar *dbus_address)
{
  IdeWorkerProcess *ret;

  IDE_ENTRY;

  g_return_val_if_fail (argv0 != NULL, NULL);
  g_return_val_if_fail (plugin_name != NULL, NULL);
  g_return_val_if_fail (dbus_address != NULL, NULL);

  ret = g_object_new (IDE_TYPE_WORKER_PROCESS,
                      "argv0", argv0,
                      "plugin-name", plugin_name,
                      "dbus-address", dbus_address,
                      NULL);

  IDE_RETURN (ret);
}

static void
ide_worker_process_wait_check_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GSubprocess *subprocess = (GSubprocess *)object;
  g_autoptr(IdeWorkerProcess) self = user_data;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_WORKER_PROCESS (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!g_subprocess_wait_check_finish (subprocess, result, &error))
    g_critical ("%s", error->message);

  g_clear_object (&self->subprocess);

  if (!self->quit)
    ide_worker_process_respawn (self);

  IDE_EXIT;
}

static void
ide_worker_process_respawn (IdeWorkerProcess *self)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  g_autofree gchar *type = NULL;
  g_autofree gchar *dbus_address = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKER_PROCESS (self));
  g_assert (self->subprocess == NULL);

  type = g_strdup_printf ("--type=%s", self->plugin_name);
  dbus_address = g_strdup_printf ("--dbus-address=%s", self->dbus_address);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  subprocess = g_subprocess_launcher_spawn (launcher, &error, self->argv0, type, dbus_address, NULL);

  if (subprocess == NULL)
    {
      g_warning ("Failed to spawn %s", error->message);
      g_clear_error (&error);
      IDE_EXIT;
    }

  self->subprocess = g_object_ref (subprocess);

  g_subprocess_wait_check_async (subprocess,
                                 NULL,
                                 ide_worker_process_wait_check_cb,
                                 g_object_ref (self));

  IDE_EXIT;
}

void
ide_worker_process_run (IdeWorkerProcess *self)
{
  g_return_if_fail (IDE_IS_WORKER_PROCESS (self));
  g_return_if_fail (self->subprocess == NULL);

  ide_worker_process_respawn (self);
}

void
ide_worker_process_quit (IdeWorkerProcess *self)
{
  g_return_if_fail (IDE_IS_WORKER_PROCESS (self));

  self->quit = TRUE;

  if (self->subprocess != NULL)
    {
      if (!g_subprocess_get_if_exited (self->subprocess))
        {
          g_autoptr(GSubprocess) subprocess = self->subprocess;

          self->subprocess = NULL;
          g_subprocess_force_exit (subprocess);
        }
    }
}

static void
ide_worker_process_dispose (GObject *object)
{
  IdeWorkerProcess *self = (IdeWorkerProcess *)object;

  if (self->subprocess != NULL)
    ide_worker_process_quit (self);

  G_OBJECT_CLASS (ide_worker_process_parent_class)->dispose (object);
}

static void
ide_worker_process_finalize (GObject *object)
{
  IdeWorkerProcess *self = (IdeWorkerProcess *)object;

  g_clear_pointer (&self->argv0, g_free);
  g_clear_pointer (&self->plugin_name, g_free);
  g_clear_pointer (&self->dbus_address, g_free);
  g_clear_object (&self->connection);
  g_clear_object (&self->subprocess);

  G_OBJECT_CLASS (ide_worker_process_parent_class)->finalize (object);

  EGG_COUNTER_DEC (instances);
}

static void
ide_worker_process_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeWorkerProcess *self = IDE_WORKER_PROCESS (object);

  switch (prop_id)
    {
    case PROP_ARGV0:
      g_value_set_string (value, self->argv0);
      break;

    case PROP_PLUGIN_NAME:
      g_value_set_string (value, self->plugin_name);
      break;

    case PROP_DBUS_ADDRESS:
      g_value_set_string (value, self->dbus_address);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_worker_process_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeWorkerProcess *self = IDE_WORKER_PROCESS (object);

  switch (prop_id)
    {
    case PROP_ARGV0:
      self->argv0 = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_NAME:
      self->plugin_name = g_value_dup_string (value);
      break;

    case PROP_DBUS_ADDRESS:
      self->dbus_address = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_worker_process_class_init (IdeWorkerProcessClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_worker_process_dispose;
  object_class->finalize = ide_worker_process_finalize;
  object_class->get_property = ide_worker_process_get_property;
  object_class->set_property = ide_worker_process_set_property;

  gParamSpecs [PROP_ARGV0] =
    g_param_spec_string ("argv0",
                         "Argv0",
                         "Argv0",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_PLUGIN_NAME] =
    g_param_spec_string ("plugin-name",
                         "plugin-name",
                         "plugin-name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_DBUS_ADDRESS] =
    g_param_spec_string ("dbus-address",
                         "dbus-address",
                         "dbus-address",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
ide_worker_process_init (IdeWorkerProcess *self)
{
  EGG_COUNTER_INC (instances);
}

gpointer
ide_worker_process_create_proxy (IdeWorkerProcess  *self,
                                 GError           **error)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  PeasExtension *exten;
  GDBusProxy *proxy;

  g_assert (IDE_IS_WORKER_PROCESS (self));
  g_assert (self->plugin_name != NULL);

  engine = peas_engine_get_default ();
  plugin_info = peas_engine_get_plugin_info (engine, self->plugin_name);

  if (plugin_info == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_PROXY_FAILED,
                   "The plugin named \"%s\" could not be found.",
                   self->plugin_name);
      return NULL;
    }

  if (self->connection == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_PROXY_FAILED,
                   "No connection has been established with the worker process");
      return NULL;
    }

  exten = peas_engine_create_extension (engine, plugin_info, IDE_TYPE_WORKER, NULL);
  proxy = ide_worker_create_proxy (IDE_WORKER (exten), self->connection);
  g_clear_object (&exten);

  return proxy;
}

gboolean
ide_worker_process_matches_credentials (IdeWorkerProcess *self,
                                        GCredentials     *credentials)
{
  g_autofree gchar *str = NULL;
  const gchar *identifier;
  pid_t pid;

  g_return_val_if_fail (IDE_IS_WORKER_PROCESS (self), FALSE);
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), FALSE);

  if ((self->subprocess != NULL) &&
      (identifier = g_subprocess_get_identifier (self->subprocess)) &&
      (pid = g_credentials_get_unix_pid (credentials, NULL)))
    {
      str = g_strdup_printf ("%d", (int)pid);
      if (g_strcmp0 (identifier, str) == 0)
        return TRUE;
    }

  return FALSE;
}

void
ide_worker_process_set_connection (IdeWorkerProcess *self,
                                   GDBusConnection  *connection)
{
  g_return_if_fail (IDE_IS_WORKER_PROCESS (self));
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  g_set_object (&self->connection, connection);
}
