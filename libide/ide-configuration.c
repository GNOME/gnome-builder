/* ide-configuration.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-configuration"

#include <string.h>

#include "ide-configuration.h"
#include "ide-context.h"
#include "ide-device.h"
#include "ide-device-manager.h"
#include "ide-environment.h"
#include "ide-runtime.h"
#include "ide-runtime-manager.h"

struct _IdeConfiguration
{
  IdeObject       parent_instance;

  gchar          *config_opts;
  gchar          *device_id;
  gchar          *display_name;
  gchar          *id;
  gchar          *prefix;
  gchar          *runtime_id;

  IdeEnvironment *environment;

  gint            parallelism;

  guint           dirty : 1;
  guint           debug : 1;
};

G_DEFINE_TYPE (IdeConfiguration, ide_configuration, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONFIG_OPTS,
  PROP_DEBUG,
  PROP_DEVICE,
  PROP_DEVICE_ID,
  PROP_DIRTY,
  PROP_DISPLAY_NAME,
  PROP_ENVIRON,
  PROP_ID,
  PROP_PARALLELISM,
  PROP_PREFIX,
  PROP_RUNTIME,
  PROP_RUNTIME_ID,
  N_PROPS
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static GParamSpec *properties [N_PROPS];
static guint signals [LAST_SIGNAL];

static void
ide_configuration_set_id (IdeConfiguration *self,
                          const gchar      *id)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (id != NULL);

  if (g_strcmp0 (id, self->id) != 0)
    {
      g_free (self->id);
      self->id = g_strdup (id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

static void
ide_configuration_device_manager_items_changed (IdeConfiguration *self,
                                                guint             position,
                                                guint             added,
                                                guint             removed,
                                                IdeDeviceManager *device_manager)
{
  IdeDevice *device;

  g_assert (IDE_IS_CONFIGURATION (self));
  g_assert (IDE_IS_DEVICE_MANAGER (device_manager));

  device = ide_device_manager_get_device (device_manager, self->device_id);

  if (device != NULL)
    ide_device_prepare_configuration (device, self);
}

static void
ide_configuration_runtime_manager_items_changed (IdeConfiguration  *self,
                                                 guint              position,
                                                 guint              added,
                                                 guint              removed,
                                                 IdeRuntimeManager *runtime_manager)
{
  IdeRuntime *runtime;

  g_assert (IDE_IS_CONFIGURATION (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));

  runtime = ide_runtime_manager_get_runtime (runtime_manager, self->runtime_id);

  if (runtime != NULL)
    ide_runtime_prepare_configuration (runtime, self);
}

static void
ide_configuration_environment_changed (IdeConfiguration *self,
                                       guint             position,
                                       guint             added,
                                       guint             removed,
                                       IdeEnvironment   *environment)
{
  g_assert (IDE_IS_CONFIGURATION (self));
  g_assert (IDE_IS_ENVIRONMENT (environment));

  ide_configuration_set_dirty (self, TRUE);
}

static void
ide_configuration_constructed (GObject *object)
{
  IdeConfiguration *self = (IdeConfiguration *)object;
  IdeContext *context;
  IdeDeviceManager *device_manager;
  IdeRuntimeManager *runtime_manager;

  G_OBJECT_CLASS (ide_configuration_parent_class)->constructed (object);

  context = ide_object_get_context (IDE_OBJECT (self));
  device_manager = ide_context_get_device_manager (context);
  runtime_manager = ide_context_get_runtime_manager (context);

  g_signal_connect_object (device_manager,
                           "items-changed",
                           G_CALLBACK (ide_configuration_device_manager_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (runtime_manager,
                           "items-changed",
                           G_CALLBACK (ide_configuration_runtime_manager_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_configuration_device_manager_items_changed (self, 0, 0, 0, device_manager);
  ide_configuration_runtime_manager_items_changed (self, 0, 0, 0, runtime_manager);
}

static void
ide_configuration_finalize (GObject *object)
{
  IdeConfiguration *self = (IdeConfiguration *)object;

  g_clear_object (&self->environment);

  g_clear_pointer (&self->config_opts, g_free);
  g_clear_pointer (&self->device_id, g_free);
  g_clear_pointer (&self->display_name, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->prefix, g_free);
  g_clear_pointer (&self->runtime_id, g_free);

  G_OBJECT_CLASS (ide_configuration_parent_class)->finalize (object);
}

static void
ide_configuration_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeConfiguration *self = IDE_CONFIGURATION (object);

  switch (prop_id)
    {
    case PROP_CONFIG_OPTS:
      g_value_set_string (value, ide_configuration_get_config_opts (self));
      break;

    case PROP_DEBUG:
      g_value_set_boolean (value, ide_configuration_get_debug (self));
      break;

    case PROP_DEVICE:
      g_value_set_object (value, ide_configuration_get_device (self));
      break;

    case PROP_DIRTY:
      g_value_set_boolean (value, ide_configuration_get_dirty (self));
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_configuration_get_display_name (self));
      break;

    case PROP_ENVIRON:
      g_value_set_boxed (value, ide_configuration_get_environ (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_configuration_get_id (self));
      break;

    case PROP_PARALLELISM:
      g_value_set_int (value, ide_configuration_get_parallelism (self));
      break;

    case PROP_PREFIX:
      g_value_set_string (value, ide_configuration_get_prefix (self));
      break;

    case PROP_RUNTIME:
      g_value_set_object (value, ide_configuration_get_runtime (self));
      break;

    case PROP_RUNTIME_ID:
      g_value_set_object (value, ide_configuration_get_runtime (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_configuration_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeConfiguration *self = IDE_CONFIGURATION (object);

  switch (prop_id)
    {
    case PROP_CONFIG_OPTS:
      ide_configuration_set_config_opts (self, g_value_get_string (value));
      break;

    case PROP_DEBUG:
      ide_configuration_set_debug (self, g_value_get_boolean (value));
      break;

    case PROP_DEVICE:
      ide_configuration_set_device (self, g_value_get_object (value));
      break;

    case PROP_DEVICE_ID:
      ide_configuration_set_device_id (self, g_value_get_string (value));
      break;

    case PROP_DIRTY:
      ide_configuration_set_dirty (self, g_value_get_boolean (value));
      break;

    case PROP_DISPLAY_NAME:
      ide_configuration_set_display_name (self, g_value_get_string (value));
      break;

    case PROP_ID:
      ide_configuration_set_id (self, g_value_get_string (value));
      break;

    case PROP_PREFIX:
      ide_configuration_set_prefix (self, g_value_get_string (value));
      break;

    case PROP_PARALLELISM:
      ide_configuration_set_parallelism (self, g_value_get_int (value));
      break;

    case PROP_RUNTIME:
      ide_configuration_set_runtime (self, g_value_get_object (value));
      break;

    case PROP_RUNTIME_ID:
      ide_configuration_set_runtime_id (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_configuration_class_init (IdeConfigurationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_configuration_constructed;
  object_class->finalize = ide_configuration_finalize;
  object_class->get_property = ide_configuration_get_property;
  object_class->set_property = ide_configuration_set_property;

  properties [PROP_CONFIG_OPTS] =
    g_param_spec_string ("config-opts",
                         "Config Options",
                         "Parameters to bootstrap the project",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEBUG] =
    g_param_spec_boolean ("debug",
                          "Debug",
                          "Debug",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "Device",
                         IDE_TYPE_DEVICE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEVICE_ID] =
    g_param_spec_string ("device-id",
                         "Device Id",
                         "The identifier of the device",
                         "local",
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DIRTY] =
    g_param_spec_boolean ("dirty",
                          "Dirty",
                          "If the configuration has been changed.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "Display Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENVIRON] =
    g_param_spec_boxed ("environ",
                        "Environ",
                        "Environ",
                        G_TYPE_STRV,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "Id",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PARALLELISM] =
    g_param_spec_int ("parallelism",
                      "Parallelism",
                      "Parallelism",
                      -1,
                      G_MAXINT,
                      -1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PREFIX] =
    g_param_spec_string ("prefix",
                         "Prefix",
                         "Prefix",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUNTIME] =
    g_param_spec_object ("runtime",
                         "Runtime",
                         "Runtime",
                         IDE_TYPE_RUNTIME,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUNTIME_ID] =
    g_param_spec_string ("runtime-id",
                         "Runtime Id",
                         "The identifier of the runtime",
                         "host",
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
ide_configuration_init (IdeConfiguration *self)
{
  self->device_id = g_strdup ("local");
  self->runtime_id = g_strdup ("host");
  self->debug = TRUE;
  self->environment = ide_environment_new ();

  g_signal_connect_object (self->environment,
                           "items-changed",
                           G_CALLBACK (ide_configuration_environment_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

IdeConfiguration *
ide_configuration_new (IdeContext  *context,
                       const gchar *id,
                       const gchar *device_id,
                       const gchar *runtime_id)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (device_id != NULL, NULL);
  g_return_val_if_fail (runtime_id != NULL, NULL);

  return g_object_new (IDE_TYPE_CONFIGURATION,
                       "context", context,
                       "device-id", device_id,
                       "id", id,
                       "runtime-id", runtime_id,
                       NULL);
}

const gchar *
ide_configuration_get_device_id (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return self->device_id;
}

void
ide_configuration_set_device_id (IdeConfiguration *self,
                                 const gchar      *device_id)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (device_id != NULL);

  if (g_strcmp0 (device_id, self->device_id) != 0)
    {
      IdeContext *context;
      IdeDeviceManager *device_manager;

      g_free (self->device_id);
      self->device_id = g_strdup (device_id);

      ide_configuration_set_dirty (self, TRUE);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEVICE_ID]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEVICE]);

      context = ide_object_get_context (IDE_OBJECT (self));
      device_manager = ide_context_get_device_manager (context);
      ide_configuration_device_manager_items_changed (self, 0, 0, 0, device_manager);
    }
}

/**
 * ide_configuration_get_device:
 * @self: An #IdeConfiguration
 *
 * Gets the device for the configuration.
 *
 * Returns: (transfer none) (nullable): An #IdeDevice.
 */
IdeDevice *
ide_configuration_get_device (IdeConfiguration *self)
{
  IdeDeviceManager *device_manager;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  device_manager = ide_context_get_device_manager (context);

  return ide_device_manager_get_device (device_manager, self->device_id);
}

void
ide_configuration_set_device (IdeConfiguration *self,
                              IdeDevice        *device)
{
  const gchar *device_id = "local";

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (!device || IDE_IS_DEVICE (device));

  if (device != NULL)
    device_id = ide_device_get_id (device);

  ide_configuration_set_device_id (self, device_id);
}

const gchar *
ide_configuration_get_runtime_id (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return self->runtime_id;
}

void
ide_configuration_set_runtime_id (IdeConfiguration *self,
                                  const gchar      *runtime_id)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (runtime_id != NULL);

  if (g_strcmp0 (runtime_id, self->runtime_id) != 0)
    {
      IdeRuntimeManager *runtime_manager;
      IdeContext *context;

      g_free (self->runtime_id);
      self->runtime_id = g_strdup (runtime_id);

      ide_configuration_set_dirty (self, TRUE);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNTIME_ID]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNTIME]);

      context = ide_object_get_context (IDE_OBJECT (self));
      runtime_manager = ide_context_get_runtime_manager (context);
      ide_configuration_runtime_manager_items_changed (self, 0, 0, 0, runtime_manager);
    }
}

/**
 * ide_configuration_get_runtime:
 * @self: An #IdeConfiguration
 *
 * Gets the runtime for the configuration.
 *
 * Returns: (transfer none) (nullable): An #IdeRuntime
 */
IdeRuntime *
ide_configuration_get_runtime (IdeConfiguration *self)
{
  IdeRuntimeManager *runtime_manager;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  runtime_manager = ide_context_get_runtime_manager (context);

  return ide_runtime_manager_get_runtime (runtime_manager, self->runtime_id);
}

void
ide_configuration_set_runtime (IdeConfiguration *self,
                               IdeRuntime       *runtime)
{
  const gchar *runtime_id = "host";

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (!runtime || IDE_IS_RUNTIME (runtime));

  if (runtime != NULL)
    runtime_id = ide_runtime_get_id (runtime);

  ide_configuration_set_runtime_id (self, runtime_id);
}

/**
 * ide_configuration_get_environ:
 * @self: An #IdeConfiguration
 *
 * Gets the environment to use when spawning processes.
 *
 * Returns: (transfer full): An array of key=value environment variables.
 */
gchar **
ide_configuration_get_environ (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return ide_environment_get_environ (self->environment);
}

const gchar *
ide_configuration_getenv (IdeConfiguration *self,
                          const gchar      *key)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return ide_environment_getenv (self->environment, key);
}

void
ide_configuration_setenv (IdeConfiguration *self,
                          const gchar      *key,
                          const gchar      *value)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (key != NULL);

  ide_environment_setenv (self->environment, key, value);
}

const gchar *
ide_configuration_get_id (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return self->id;
}

const gchar *
ide_configuration_get_prefix (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return self->prefix;
}

void
ide_configuration_set_prefix (IdeConfiguration *self,
                              const gchar      *prefix)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (g_strcmp0 (prefix, self->prefix) != 0)
    {
      g_free (self->prefix);
      self->prefix = g_strdup (prefix);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PREFIX]);
      ide_configuration_set_dirty (self, TRUE);
    }
}

gint
ide_configuration_get_parallelism (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), -1);

  return self->parallelism;
}

void
ide_configuration_set_parallelism (IdeConfiguration *self,
                                   gint              parallelism)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (parallelism >= -1);

  if (parallelism != self->parallelism)
    {
      self->parallelism = parallelism;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PARALLELISM]);
    }
}

gboolean
ide_configuration_get_debug (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), FALSE);

  return self->debug;
}

void
ide_configuration_set_debug (IdeConfiguration *self,
                             gboolean          debug)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  debug = !!debug;

  if (debug != self->debug)
    {
      self->debug = debug;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUG]);
      ide_configuration_set_dirty (self, TRUE);
    }
}

const gchar *
ide_configuration_get_display_name (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return self->display_name;
}

void
ide_configuration_set_display_name (IdeConfiguration *self,
                                    const gchar      *display_name)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (g_strcmp0 (display_name, self->display_name) != 0)
    {
      g_free (self->display_name);
      self->display_name = g_strdup (display_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
    }
}

gboolean
ide_configuration_get_dirty (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), FALSE);

  return self->dirty;
}

void
ide_configuration_set_dirty (IdeConfiguration *self,
                             gboolean          dirty)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  dirty = !!dirty;

  if (dirty != self->dirty)
    {
      self->dirty = dirty;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRTY]);
    }

  /*
   * Always emit the changed signal so that the configuration manager
   * can queue a writeback of the configuration.
   */
  g_signal_emit (self, signals [CHANGED], 0);
}

/**
 * ide_configuration_get_environment:
 *
 * Returns: (transfer none): An #IdeEnvironment.
 */
IdeEnvironment *
ide_configuration_get_environment (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return self->environment;
}

const gchar *
ide_configuration_get_config_opts (IdeConfiguration *self)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return self->config_opts;
}

void
ide_configuration_set_config_opts (IdeConfiguration *self,
                                   const gchar      *config_opts)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (g_strcmp0 (config_opts, self->config_opts) != 0)
    {
      g_free (self->config_opts);
      self->config_opts = g_strdup (config_opts);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONFIG_OPTS]);
      ide_configuration_set_dirty (self, TRUE);
    }
}

/**
 * ide_configuration_duplicate:
 * @self: An #IdeConfiguration
 *
 * Copies the configuration into a new configuration.
 *
 * Returns: (transfer full): An #IdeConfiguration.
 */
IdeConfiguration *
ide_configuration_duplicate (IdeConfiguration *self)
{
  static gint next_counter = 2;
  IdeConfiguration *copy;
  IdeContext *context;
  g_autofree gchar *id = NULL;
  g_autofree gchar *name = NULL;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  id = g_strdup_printf ("%s %d", self->id, next_counter++);
  name = g_strdup_printf ("%s Copy", self->display_name);

  copy = g_object_new (IDE_TYPE_CONFIGURATION,
                       "config-opts", self->config_opts,
                       "context", context,
                       "device-id", self->device_id,
                       "display-name", name,
                       "id", id,
                       "prefix", self->prefix,
                       "runtime-id", self->runtime_id,
                       NULL);

  copy->environment = ide_environment_copy (self->environment);

  return copy;
}
