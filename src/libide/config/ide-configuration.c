/* ide-configuration.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-enums.h"

#include "config/ide-configuration.h"
#include "config/ide-configuration-manager.h"
#include "buildsystem/ide-environment.h"
#include "devices/ide-device-manager.h"
#include "devices/ide-device.h"
#include "runtimes/ide-runtime-manager.h"
#include "runtimes/ide-runtime.h"
#include "subprocess/ide-subprocess-launcher.h"

typedef struct
{
  gchar          *app_id;
  gchar         **build_commands;
  gchar          *config_opts;
  gchar          *device_id;
  gchar          *display_name;
  gchar          *id;
  gchar         **post_install_commands;
  gchar          *prefix;
  gchar          *run_opts;
  gchar          *runtime_id;
  gchar          *append_path;

  IdeEnvironment *environment;

  GHashTable     *internal;

  gint            parallelism;
  guint           sequence;

  guint           dirty : 1;
  guint           debug : 1;

  /*
   * These are used to determine if we can make progress building
   * with this configuration. When devices are added/removed, the
   * IdeConfiguration:ready property will be notified.
   */
  guint           device_ready : 1;
  guint           runtime_ready : 1;

  IdeBuildLocality locality : 3;
} IdeConfigurationPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeConfiguration, ide_configuration, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_APPEND_PATH,
  PROP_APP_ID,
  PROP_BUILD_COMMANDS,
  PROP_CONFIG_OPTS,
  PROP_DEBUG,
  PROP_DEVICE,
  PROP_DEVICE_ID,
  PROP_DIRTY,
  PROP_DISPLAY_NAME,
  PROP_ENVIRON,
  PROP_ID,
  PROP_LOCALITY,
  PROP_PARALLELISM,
  PROP_POST_INSTALL_COMMANDS,
  PROP_PREFIX,
  PROP_READY,
  PROP_RUNTIME,
  PROP_RUNTIME_ID,
  PROP_RUN_OPTS,
  N_PROPS
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static GParamSpec *properties [N_PROPS];
static guint signals [LAST_SIGNAL];

static void
_value_free (gpointer data)
{
  GValue *value = data;

  if (value != NULL)
    {
      g_value_unset (value);
      g_slice_free (GValue, value);
    }
}

static GValue *
_value_new (GType type)
{
  GValue *value;

  value = g_slice_new0 (GValue);
  g_value_init (value, type);

  return value;
}

static void
ide_configuration_emit_changed (IdeConfiguration *self)
{
  g_assert (IDE_IS_CONFIGURATION (self));

  g_signal_emit (self, signals [CHANGED], 0);
}

static IdeDevice *
ide_configuration_real_get_device (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  if (priv->device_id != NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeDeviceManager *device_manager = ide_context_get_device_manager (context);
      IdeDevice *device = ide_device_manager_get_device (device_manager, priv->device_id);

      if (device != NULL)
        return g_object_ref (device);
    }

  return NULL;
}

static IdeRuntime *
ide_configuration_real_get_runtime (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  if (priv->runtime_id != NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeRuntimeManager *runtime_manager = ide_context_get_runtime_manager (context);
      IdeRuntime *runtime = ide_runtime_manager_get_runtime (runtime_manager, priv->runtime_id);

      if (runtime != NULL)
        return g_object_ref (runtime);
    }

  return NULL;
}

static void
ide_configuration_set_id (IdeConfiguration *self,
                          const gchar      *id)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (id != NULL);

  if (g_strcmp0 (id, priv->id) != 0)
    {
      g_free (priv->id);
      priv->id = g_strdup (id);
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
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);
  IdeDevice *device;
  gboolean device_ready;

  g_assert (IDE_IS_CONFIGURATION (self));
  g_assert (IDE_IS_DEVICE_MANAGER (device_manager));

  device = ide_device_manager_get_device (device_manager, priv->device_id);
  device_ready = !!device;

  if (!priv->device_ready && device_ready)
    ide_device_prepare_configuration (device, self);

  if (device_ready != priv->device_ready)
    {
      priv->device_ready = device_ready;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READY]);
    }
}

static void
ide_configuration_runtime_manager_items_changed (IdeConfiguration  *self,
                                                 guint              position,
                                                 guint              added,
                                                 guint              removed,
                                                 IdeRuntimeManager *runtime_manager)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);
  IdeRuntime *runtime;
  gboolean runtime_ready;

  g_assert (IDE_IS_CONFIGURATION (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));

  runtime = ide_runtime_manager_get_runtime (runtime_manager, priv->runtime_id);
  runtime_ready = !!runtime;

  if (!priv->runtime_ready && runtime_ready)
    ide_runtime_prepare_configuration (runtime, self);

  if (runtime_ready != priv->runtime_ready)
    {
      priv->runtime_ready = runtime_ready;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_READY]);
    }
}

static void
ide_configuration_environment_changed (IdeConfiguration *self,
                                       IdeEnvironment   *environment)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CONFIGURATION (self));
  g_assert (IDE_IS_ENVIRONMENT (environment));

  ide_configuration_set_dirty (self, TRUE);
  ide_configuration_emit_changed (self);

  IDE_EXIT;
}

static void
ide_configuration_real_set_device (IdeConfiguration *self,
                                    IdeDevice       *device)
{
  const gchar *device_id = "local";

  g_assert (IDE_IS_CONFIGURATION (self));
  g_assert (!device || IDE_IS_DEVICE (device));

  if (device != NULL)
    device_id = ide_device_get_id (device);

  ide_configuration_set_device_id (self, device_id);
}

static void
ide_configuration_real_set_runtime (IdeConfiguration *self,
                                    IdeRuntime       *runtime)
{
  const gchar *runtime_id = "host";

  g_assert (IDE_IS_CONFIGURATION (self));
  g_assert (!runtime || IDE_IS_RUNTIME (runtime));

  if (runtime != NULL)
    runtime_id = ide_runtime_get_id (runtime);

  ide_configuration_set_runtime_id (self, runtime_id);
}

static void
ide_configuration_constructed (GObject *object)
{
  IdeConfiguration *self = (IdeConfiguration *)object;
  IdeContext *context;
  IdeDeviceManager *device_manager;
  IdeRuntimeManager *runtime_manager;

  G_OBJECT_CLASS (ide_configuration_parent_class)->constructed (object);

  /* Allow ourselves to be run from unit tests without a valid context */
  if (NULL != (context = ide_object_get_context (IDE_OBJECT (self))))
    {
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
}

static void
ide_configuration_finalize (GObject *object)
{
  IdeConfiguration *self = (IdeConfiguration *)object;
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_clear_object (&priv->environment);

  g_clear_pointer (&priv->build_commands, g_strfreev);
  g_clear_pointer (&priv->internal, g_hash_table_unref);
  g_clear_pointer (&priv->config_opts, g_free);
  g_clear_pointer (&priv->device_id, g_free);
  g_clear_pointer (&priv->display_name, g_free);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->post_install_commands, g_strfreev);
  g_clear_pointer (&priv->prefix, g_free);
  g_clear_pointer (&priv->runtime_id, g_free);
  g_clear_pointer (&priv->app_id, g_free);

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

    case PROP_BUILD_COMMANDS:
      g_value_set_boxed (value, ide_configuration_get_build_commands (self));
      break;

    case PROP_DEBUG:
      g_value_set_boolean (value, ide_configuration_get_debug (self));
      break;

    case PROP_DEVICE:
      g_value_set_object (value, ide_configuration_get_device (self));
      break;

    case PROP_DEVICE_ID:
      g_value_set_string (value, ide_configuration_get_device_id (self));
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

    case PROP_READY:
      g_value_set_boolean (value, ide_configuration_get_ready (self));
      break;

    case PROP_POST_INSTALL_COMMANDS:
      g_value_set_boxed (value, ide_configuration_get_post_install_commands (self));
      break;

    case PROP_PREFIX:
      g_value_set_string (value, ide_configuration_get_prefix (self));
      break;

    case PROP_RUNTIME:
      g_value_set_object (value, ide_configuration_get_runtime (self));
      break;

    case PROP_RUNTIME_ID:
      g_value_set_string (value, ide_configuration_get_runtime_id (self));
      break;

    case PROP_RUN_OPTS:
      g_value_set_string (value, ide_configuration_get_run_opts (self));
      break;

    case PROP_APP_ID:
      g_value_set_string (value, ide_configuration_get_app_id (self));
      break;

    case PROP_APPEND_PATH:
      g_value_set_string (value, ide_configuration_get_append_path (self));
      break;

    case PROP_LOCALITY:
      g_value_set_flags (value, ide_configuration_get_locality (self));
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

    case PROP_BUILD_COMMANDS:
      ide_configuration_set_build_commands (self, g_value_get_boxed (value));
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

    case PROP_POST_INSTALL_COMMANDS:
      ide_configuration_set_post_install_commands (self, g_value_get_boxed (value));
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

    case PROP_RUN_OPTS:
      ide_configuration_set_run_opts (self, g_value_get_string (value));
      break;

    case PROP_APP_ID:
      ide_configuration_set_app_id (self, g_value_get_string (value));
      break;

    case PROP_APPEND_PATH:
      ide_configuration_set_append_path (self, g_value_get_string (value));
      break;

    case PROP_LOCALITY:
      ide_configuration_set_locality (self, g_value_get_flags (value));
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

  klass->get_device = ide_configuration_real_get_device;
  klass->set_device = ide_configuration_real_set_device;
  klass->get_runtime = ide_configuration_real_get_runtime;
  klass->set_runtime = ide_configuration_real_set_runtime;

  properties [PROP_APPEND_PATH] =
    g_param_spec_string ("append-path",
                         "Append Path",
                         "Append to PATH environment variable",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_BUILD_COMMANDS] =
    g_param_spec_boxed ("build-commands",
                        "Build commands",
                        "Build commands",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONFIG_OPTS] =
    g_param_spec_string ("config-opts",
                         "Config Options",
                         "Parameters to bootstrap the project",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEBUG] =
    g_param_spec_boolean ("debug",
                          "Debug",
                          "Debug",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "Device",
                         IDE_TYPE_DEVICE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEVICE_ID] =
    g_param_spec_string ("device-id",
                         "Device Id",
                         "The identifier of the device",
                         "local",
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DIRTY] =
    g_param_spec_boolean ("dirty",
                          "Dirty",
                          "If the configuration has been changed.",
                          FALSE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "Display Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENVIRON] =
    g_param_spec_boxed ("environ",
                        "Environ",
                        "Environ",
                        G_TYPE_STRV,
                        (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

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
                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_POST_INSTALL_COMMANDS] =
    g_param_spec_boxed ("post-install-commands",
                        "Post install commands",
                        "Post install commands",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PREFIX] =
    g_param_spec_string ("prefix",
                         "Prefix",
                         "Prefix",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_READY] =
    g_param_spec_boolean ("ready",
                          "Ready",
                          "If the configuration can be used for building",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUN_OPTS] =
    g_param_spec_string ("run-opts",
                         "Run Options",
                         "The options for running the target application",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUNTIME] =
    g_param_spec_object ("runtime",
                         "Runtime",
                         "Runtime",
                         IDE_TYPE_RUNTIME,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUNTIME_ID] =
    g_param_spec_string ("runtime-id",
                         "Runtime Id",
                         "The identifier of the runtime",
                         "host",
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_APP_ID] =
    g_param_spec_string ("app-id",
                         "App ID",
                         "The application ID (such as org.gnome.Builder)",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LOCALITY] =
    g_param_spec_flags ("locality",
                        "Locality",
                        "Where the build may occur",
                        IDE_TYPE_BUILD_LOCALITY,
                        IDE_BUILD_LOCALITY_DEFAULT,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

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
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  priv->device_id = g_strdup ("local");
  priv->runtime_id = g_strdup ("host");
  priv->debug = TRUE;
  priv->environment = ide_environment_new ();
  priv->parallelism = -1;
  priv->locality = IDE_BUILD_LOCALITY_DEFAULT;

  priv->internal = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, _value_free);

  g_signal_connect_object (priv->environment,
                           "changed",
                           G_CALLBACK (ide_configuration_environment_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

const gchar *
ide_configuration_get_device_id (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return priv->device_id;
}

void
ide_configuration_set_device_id (IdeConfiguration *self,
                                 const gchar      *device_id)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (device_id != NULL);

  if (device_id == NULL)
    device_id = "local";

  if (g_strcmp0 (device_id, priv->device_id) != 0)
    {
      IdeContext *context;
      IdeDeviceManager *device_manager;

      g_free (priv->device_id);
      priv->device_id = g_strdup (device_id);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEVICE_ID]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEVICE]);

      context = ide_object_get_context (IDE_OBJECT (self));
      device_manager = ide_context_get_device_manager (context);
      ide_configuration_device_manager_items_changed (self, 0, 0, 0, device_manager);

      ide_configuration_set_dirty (self, TRUE);
      ide_configuration_emit_changed (self);
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
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return IDE_CONFIGURATION_GET_CLASS (self)->get_device (self);
}

void
ide_configuration_set_device (IdeConfiguration *self,
                              IdeDevice        *device)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (!device || IDE_IS_DEVICE (device));

  IDE_CONFIGURATION_GET_CLASS (self)->set_device (self, device);
}

/**
 * ide_configuration_get_app_id:
 * @self: An #IdeConfiguration
 *
 * Gets the application ID for the configuration.
 *
 * Returns: (transfer none) (nullable): A string.
 */
const gchar *
ide_configuration_get_app_id (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return priv->app_id;
}

void
ide_configuration_set_app_id (IdeConfiguration *self,
                              const gchar      *app_id)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (priv->app_id != app_id)
    {
      g_free (priv->app_id);
      priv->app_id = g_strdup (app_id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_APP_ID]);
    }
}

const gchar *
ide_configuration_get_runtime_id (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return priv->runtime_id;
}

void
ide_configuration_set_runtime_id (IdeConfiguration *self,
                                  const gchar      *runtime_id)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (runtime_id == NULL)
    runtime_id = "host";

  if (g_strcmp0 (runtime_id, priv->runtime_id) != 0)
    {
      IdeRuntimeManager *runtime_manager;
      IdeContext *context;
      IdeRuntime *runtime;

      g_free (priv->runtime_id);
      priv->runtime_id = g_strdup (runtime_id);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNTIME_ID]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNTIME]);

      context = ide_object_get_context (IDE_OBJECT (self));
      runtime_manager = ide_context_get_runtime_manager (context);
      ide_configuration_runtime_manager_items_changed (self, 0, 0, 0, runtime_manager);

      runtime = ide_configuration_get_runtime (self);
      if (runtime != NULL)
        ide_runtime_prepare_configuration (runtime, self);

      ide_configuration_set_dirty (self, TRUE);
      ide_configuration_emit_changed (self);
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
  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return IDE_CONFIGURATION_GET_CLASS (self)->get_runtime (self);
}

void
ide_configuration_set_runtime (IdeConfiguration *self,
                               IdeRuntime       *runtime)
{
  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (!runtime || IDE_IS_RUNTIME (runtime));

  IDE_CONFIGURATION_GET_CLASS (self)->set_runtime (self, runtime);
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
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return ide_environment_get_environ (priv->environment);
}

const gchar *
ide_configuration_getenv (IdeConfiguration *self,
                          const gchar      *key)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return ide_environment_getenv (priv->environment, key);
}

void
ide_configuration_setenv (IdeConfiguration *self,
                          const gchar      *key,
                          const gchar      *value)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (key != NULL);

  ide_environment_setenv (priv->environment, key, value);
}

const gchar *
ide_configuration_get_id (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return priv->id;
}

const gchar *
ide_configuration_get_prefix (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return priv->prefix;
}

void
ide_configuration_set_prefix (IdeConfiguration *self,
                              const gchar      *prefix)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (g_strcmp0 (prefix, priv->prefix) != 0)
    {
      g_free (priv->prefix);
      priv->prefix = g_strdup (prefix);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PREFIX]);
      ide_configuration_set_dirty (self, TRUE);
    }
}

gint
ide_configuration_get_parallelism (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), -1);

  if (priv->parallelism == -1)
    {
      g_autoptr(GSettings) settings = g_settings_new ("org.gnome.builder.build");

      return g_settings_get_int (settings, "parallel");
    }

  return priv->parallelism;
}

void
ide_configuration_set_parallelism (IdeConfiguration *self,
                                   gint              parallelism)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (parallelism >= -1);

  if (parallelism != priv->parallelism)
    {
      priv->parallelism = parallelism;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PARALLELISM]);
    }
}

gboolean
ide_configuration_get_debug (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), FALSE);

  return priv->debug;
}

void
ide_configuration_set_debug (IdeConfiguration *self,
                             gboolean          debug)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  debug = !!debug;

  if (debug != priv->debug)
    {
      priv->debug = debug;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUG]);
      ide_configuration_set_dirty (self, TRUE);
    }
}

const gchar *
ide_configuration_get_display_name (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return priv->display_name;
}

void
ide_configuration_set_display_name (IdeConfiguration *self,
                                    const gchar      *display_name)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (g_strcmp0 (display_name, priv->display_name) != 0)
    {
      g_free (priv->display_name);
      priv->display_name = g_strdup (display_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
      ide_configuration_emit_changed (self);
    }
}

gboolean
ide_configuration_get_dirty (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), FALSE);

  return priv->dirty;
}

void
ide_configuration_set_dirty (IdeConfiguration *self,
                             gboolean          dirty)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  dirty = !!dirty;

  if (dirty != priv->dirty)
    {
      priv->dirty = dirty;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRTY]);
    }

  if (dirty)
    {
      /*
       * Emit the changed signal so that the configuration manager
       * can queue a writeback of the configuration. If we are
       * clearing the dirty bit, then we don't need to do this.
       */
      priv->sequence++;
      IDE_TRACE_MSG ("configuration set dirty with sequence %u", priv->sequence);
      ide_configuration_emit_changed (self);
    }

  IDE_EXIT;
}

/**
 * ide_configuration_get_environment:
 *
 * Returns: (transfer none): An #IdeEnvironment.
 */
IdeEnvironment *
ide_configuration_get_environment (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return priv->environment;
}

void
ide_configuration_set_environment (IdeConfiguration *self,
                                   IdeEnvironment   *environment)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  g_clear_object (&priv->environment);
  priv->environment = g_object_ref (environment);
  g_signal_connect_object (priv->environment,
                           "changed",
                           G_CALLBACK (ide_configuration_environment_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

const gchar *
ide_configuration_get_config_opts (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return priv->config_opts;
}

void
ide_configuration_set_config_opts (IdeConfiguration *self,
                                   const gchar      *config_opts)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (g_strcmp0 (config_opts, priv->config_opts) != 0)
    {
      g_free (priv->config_opts);
      priv->config_opts = g_strdup (config_opts);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONFIG_OPTS]);
      ide_configuration_set_dirty (self, TRUE);
    }
}

const gchar * const *
ide_configuration_get_build_commands (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return (const gchar * const *)priv->build_commands;
}

void
ide_configuration_set_build_commands (IdeConfiguration *self,
                                      const gchar * const     *build_commands)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (priv->build_commands != (gchar **)build_commands)
    {
      g_strfreev (priv->build_commands);
      priv->build_commands = g_strdupv ((gchar **)build_commands);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUILD_COMMANDS]);
    }
}

const gchar * const *
ide_configuration_get_post_install_commands (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return (const gchar * const *)priv->post_install_commands;
}

void
ide_configuration_set_post_install_commands (IdeConfiguration    *self,
                                             const gchar * const *post_install_commands)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (priv->post_install_commands != (gchar **)post_install_commands)
    {
      g_strfreev (priv->post_install_commands);
      priv->post_install_commands = g_strdupv ((gchar **)post_install_commands);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_POST_INSTALL_COMMANDS]);
    }
}

/**
 * ide_configuration_get_sequence:
 * @self: An #IdeConfiguration
 *
 * This returns a sequence number for the configuration. This is useful
 * for build systems that want to clear the "dirty" bit on the configuration
 * so that they need not bootstrap a second time. This should be done by
 * checking the sequence number before executing the bootstrap, and only
 * cleared if the sequence number matches after performing the bootstrap.
 * This indicates no changes have been made to the configuration in the
 * mean time.
 *
 * Returns: A monotonic sequence number.
 */
guint
ide_configuration_get_sequence (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), 0);

  return priv->sequence;
}

static GValue *
ide_configuration_reset_internal_value (IdeConfiguration *self,
                                        const gchar      *key,
                                        GType             type)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);
  GValue *v;

  g_assert (IDE_IS_CONFIGURATION (self));
  g_assert (key != NULL);
  g_assert (type != G_TYPE_INVALID);

  v = g_hash_table_lookup (priv->internal, key);

  if (v == NULL)
    {
      v = _value_new (type);
      g_hash_table_insert (priv->internal, g_strdup (key), v);
    }
  else
    {
      g_value_unset (v);
      g_value_init (v, type);
    }

  return v;
}

const gchar *
ide_configuration_get_internal_string (IdeConfiguration *self,
                                       const gchar      *key)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS_STRING (v))
    return g_value_get_string (v);

  return NULL;
}

void
ide_configuration_set_internal_string (IdeConfiguration *self,
                                       const gchar      *key,
                                       const gchar      *value)
{
  GValue *v;

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (key != NULL);

  v = ide_configuration_reset_internal_value (self, key, G_TYPE_STRING);
  g_value_set_string (v, value);
}

const gchar * const *
ide_configuration_get_internal_strv (IdeConfiguration *self,
                                     const gchar      *key)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS (v, G_TYPE_STRV))
    return g_value_get_boxed (v);

  return NULL;
}

void
ide_configuration_set_internal_strv (IdeConfiguration    *self,
                                     const gchar         *key,
                                     const gchar * const *value)
{
  GValue *v;

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (key != NULL);

  v = ide_configuration_reset_internal_value (self, key, G_TYPE_STRV);
  g_value_set_boxed (v, value);
}

gboolean
ide_configuration_get_internal_boolean (IdeConfiguration *self,
                                        const gchar      *key)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS_BOOLEAN (v))
    return g_value_get_boolean (v);

  return FALSE;
}

void
ide_configuration_set_internal_boolean (IdeConfiguration  *self,
                                        const gchar       *key,
                                        gboolean           value)
{
  GValue *v;

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (key != NULL);

  v = ide_configuration_reset_internal_value (self, key, G_TYPE_BOOLEAN);
  g_value_set_boolean (v, value);
}

gint
ide_configuration_get_internal_int (IdeConfiguration *self,
                                    const gchar      *key)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), -1);
  g_return_val_if_fail (key != NULL, -1);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS_INT (v))
    return g_value_get_int (v);

  return 0;
}

void
ide_configuration_set_internal_int (IdeConfiguration *self,
                                    const gchar      *key,
                                    gint              value)
{
  GValue *v;

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (key != NULL);

  v = ide_configuration_reset_internal_value (self, key, G_TYPE_INT);
  g_value_set_int (v, value);
}

gint64
ide_configuration_get_internal_int64 (IdeConfiguration *self,
                                      const gchar      *key)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), -1);
  g_return_val_if_fail (key != NULL, -1);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS_INT64 (v))
    return g_value_get_int64 (v);

  return 0;
}

void
ide_configuration_set_internal_int64 (IdeConfiguration *self,
                                      const gchar      *key,
                                      gint64            value)
{
  GValue *v;

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (key != NULL);

  v = ide_configuration_reset_internal_value (self, key, G_TYPE_INT64);
  g_value_set_int64 (v, value);
}

/**
 * ide_configuration_get_internal_object:
 * @self: An #IdeConfiguration
 * @key: The key to get
 *
 * Gets the value associated with @key if it is a #GObject.
 *
 * Returns: (nullable) (transfer none) (type GObject.Object): a #GObject or %NULL.
 */
gpointer
ide_configuration_get_internal_object (IdeConfiguration *self,
                                       const gchar      *key)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS_OBJECT (v))
    return g_value_get_object (v);

  return NULL;
}

/**
 * ide_configuration_set_internal_object:
 * @self: an #IdeConfiguration
 * @key: the key to set
 * @instance: (type GObject.Object) (nullable): a #GObject or %NULL
 *
 * Sets the value for @key to @instance.
 */
void
ide_configuration_set_internal_object (IdeConfiguration *self,
                                       const gchar      *key,
                                       gpointer          instance)
{
  GValue *v;
  GType type;

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (key != NULL);

  if (instance != NULL)
    type = G_OBJECT_TYPE (instance);
  else
    type = G_TYPE_OBJECT;

  v = ide_configuration_reset_internal_value (self, key, type);
  g_value_set_object (v, instance);
}

/**
 * ide_configuration_get_ready:
 * @self: An #IdeConfiguration
 *
 * Determines if the configuration is ready for use. That means that the
 * build device can be accessed and the runtime is loaded. This may change
 * at runtime as devices and runtimes are added or removed.
 *
 * Returns: %TRUE if the configuration is ready for use.
 */
gboolean
ide_configuration_get_ready (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), FALSE);

  return priv->device_ready && priv->runtime_ready;
}

gboolean
ide_configuration_supports_device (IdeConfiguration *self,
                                   IdeDevice        *device)
{
  gboolean ret = TRUE;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), FALSE);
  g_return_val_if_fail (IDE_IS_DEVICE (device), FALSE);

  if (IDE_CONFIGURATION_GET_CLASS (self)->supports_device)
    ret = IDE_CONFIGURATION_GET_CLASS (self)->supports_device (self, device);

  IDE_RETURN (ret);
}

gboolean
ide_configuration_supports_runtime (IdeConfiguration *self,
                                    IdeRuntime       *runtime)
{
  gboolean ret = TRUE;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), FALSE);
  g_return_val_if_fail (IDE_IS_RUNTIME (runtime), FALSE);

  if (IDE_CONFIGURATION_GET_CLASS (self)->supports_runtime)
    ret = IDE_CONFIGURATION_GET_CLASS (self)->supports_runtime (self, runtime);

  IDE_RETURN (ret);
}

/**
 * ide_configuration_get_run_opts:
 * @self: a #IdeConfiguration
 *
 * Gets the command line options to use when running the target application.
 * The result should be parsed with g_shell_parse_argv() to convert the run
 * options to an array suitable for use in argv.
 *
 * Returns: (transfer none) (nullable): A string containing the run options
 *   or %NULL if none have been set.
 *
 * Since: 3.26
 */
const gchar *
ide_configuration_get_run_opts (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return priv->run_opts;
}

/**
 * ide_configuration_set_run_opts:
 * @self: a #IdeConfiguration
 * @run_opts: (nullable): the run options for the target application
 *
 * Sets the run options to use when running the target application.
 * See ide_configuration_get_run_opts() for more information.
 *
 * Since: 3.26
 */
void
ide_configuration_set_run_opts (IdeConfiguration *self,
                                const gchar      *run_opts)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (g_strcmp0 (run_opts, priv->run_opts) != 0)
    {
      g_free (priv->run_opts);
      priv->run_opts = g_strdup (run_opts);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUN_OPTS]);
    }
}

const gchar *
ide_configuration_get_append_path (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), NULL);

  return priv->append_path;
}

void
ide_configuration_set_append_path (IdeConfiguration *self,
                                   const gchar      *append_path)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));

  if (priv->append_path != append_path)
    {
      g_free (priv->append_path);
      priv->append_path = g_strdup (append_path);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_APPEND_PATH]);
    }
}

void
ide_configuration_apply_path (IdeConfiguration      *self,
                              IdeSubprocessLauncher *launcher)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  if (priv->append_path != NULL)
    ide_subprocess_launcher_append_path (launcher, priv->append_path);
}

IdeBuildLocality
ide_configuration_get_locality (IdeConfiguration *self)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIGURATION (self), 0);

  return priv->locality;
}

void
ide_configuration_set_locality (IdeConfiguration *self,
                                IdeBuildLocality  locality)
{
  IdeConfigurationPrivate *priv = ide_configuration_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIGURATION (self));
  g_return_if_fail (locality > 0);
  g_return_if_fail (locality <= IDE_BUILD_LOCALITY_DEFAULT);

  if (priv->locality != locality)
    {
      priv->locality = locality;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOCALITY]);
    }
}
