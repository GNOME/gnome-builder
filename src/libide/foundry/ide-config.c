/* ide-config.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-config"

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>

#include "ide-config-manager.h"
#include "ide-config-private.h"
#include "ide-config.h"
#include "ide-foundry-enums.h"
#include "ide-foundry-compat.h"
#include "ide-runtime-manager.h"
#include "ide-runtime.h"
#include "ide-toolchain-manager.h"
#include "ide-toolchain.h"

typedef struct
{
  gchar          *app_id;
  gchar         **build_commands;
  gchar          *config_opts;
  gchar          *display_name;
  gchar          *id;
  gchar         **post_install_commands;
  gchar          *prefix;
  gchar          *run_opts;
  gchar          *runtime_id;
  gchar          *toolchain_id;
  gchar          *prepend_path;
  gchar          *append_path;
  GHashTable     *pipeline_args;

  GFile          *build_commands_dir;

  IdeEnvironment *environment;
  IdeEnvironment *runtime_environment;

  GHashTable     *internal;

  gint            parallelism;
  guint           sequence;

  guint           block_changed;

  guint           dirty : 1;
  guint           debug : 1;
  guint           has_attached : 1;
  guint           prefix_set : 1;

  /*
   * This is used to determine if we can make progress building
   * with this configuration. When runtimes are added/removed, the
   * IdeConfig:ready property will be notified.
   */
  guint           runtime_ready : 1;

  IdeBuildLocality locality : 3;
} IdeConfigPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeConfig, ide_config, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PREPEND_PATH,
  PROP_APPEND_PATH,
  PROP_APP_ID,
  PROP_BUILD_COMMANDS,
  PROP_BUILD_COMMANDS_DIR,
  PROP_CONFIG_OPTS,
  PROP_DEBUG,
  PROP_DIRTY,
  PROP_DISPLAY_NAME,
  PROP_ENVIRON,
  PROP_ID,
  PROP_LOCALITY,
  PROP_PARALLELISM,
  PROP_POST_INSTALL_COMMANDS,
  PROP_PREFIX,
  PROP_PREFIX_SET,
  PROP_READY,
  PROP_RUNTIME,
  PROP_RUNTIME_ID,
  PROP_TOOLCHAIN_ID,
  PROP_TOOLCHAIN,
  PROP_RUN_OPTS,
  PROP_SUPPORTED_RUNTIMES,
  PROP_DESCRIPTION,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

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
ide_config_block_changed (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_assert (IDE_IS_CONFIG (self));

  priv->block_changed++;
}

static void
ide_config_unblock_changed (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_assert (IDE_IS_CONFIG (self));

  priv->block_changed--;
}

static void
ide_config_emit_changed (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_assert (IDE_IS_CONFIG (self));

  if (priv->block_changed == 0)
    g_signal_emit (self, signals [CHANGED], 0);
}

static GFile *
ide_config_real_translate_file (IdeConfig *self,
                                GFile     *file)
{
  IdeRuntime *runtime;

  g_assert (IDE_IS_CONFIG (self));
  g_assert (G_IS_FILE (file));

  if ((runtime = ide_config_get_runtime (self)))
    return ide_runtime_translate_file (runtime, file);

  return g_object_ref (file);
}

static IdeRuntime *
ide_config_real_get_runtime (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  if (priv->runtime_id != NULL)
    {
      g_autoptr(IdeContext) context = NULL;
      g_autoptr(IdeRuntimeManager) runtime_manager = NULL;

      /* We might be in a thread, ref objects */
      context = ide_object_ref_context (IDE_OBJECT (self));
      runtime_manager = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_RUNTIME_MANAGER);

      return ide_runtime_manager_get_runtime (runtime_manager, priv->runtime_id);
    }

  return NULL;
}

static void
ide_config_set_id (IdeConfig   *self,
                   const gchar *id)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (id != NULL);

  if (g_set_str (&priv->id, id))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

static void
ide_config_runtime_manager_items_changed (IdeConfig         *self,
                                          guint              position,
                                          guint              added,
                                          guint              removed,
                                          IdeRuntimeManager *runtime_manager)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  IdeRuntime *runtime;
  gboolean runtime_ready;

  g_assert (IDE_IS_CONFIG (self));

  if (ide_object_in_destruction (IDE_OBJECT (self)))
    return;

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
ide_config_environment_changed (IdeConfig      *self,
                                IdeEnvironment *environment)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CONFIG (self));
  g_assert (IDE_IS_ENVIRONMENT (environment));

  if (ide_object_in_destruction (IDE_OBJECT (self)))
    return;

  ide_config_set_dirty (self, TRUE);

  IDE_EXIT;
}

static void
ide_config_runtime_environment_changed (IdeConfig      *self,
                                        IdeEnvironment *environment)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CONFIG (self));
  g_assert (IDE_IS_ENVIRONMENT (environment));

  if (ide_object_in_destruction (IDE_OBJECT (self)))
    return;

  ide_config_set_dirty (self, TRUE);

  IDE_EXIT;
}

static void
ide_config_real_set_runtime (IdeConfig  *self,
                             IdeRuntime *runtime)
{
  const gchar *runtime_id = "host";

  g_assert (IDE_IS_CONFIG (self));
  g_assert (!runtime || IDE_IS_RUNTIME (runtime));

  if (runtime != NULL)
    runtime_id = ide_runtime_get_id (runtime);

  ide_config_set_runtime_id (self, runtime_id);
}

static gboolean
filter_supported_runtime_cb (gpointer item,
                             gpointer user_data)
{
  return ide_config_supports_runtime (user_data, item);
}

static GListModel *
ide_config_get_supported_runtimes (IdeConfig *self)
{
  IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
  GListModel *runtimes = G_LIST_MODEL (ide_runtime_manager_from_context (context));
  GtkCustomFilter *filter = gtk_custom_filter_new (filter_supported_runtime_cb, g_object_ref (self), g_object_unref);

  return G_LIST_MODEL (gtk_filter_list_model_new (g_object_ref (runtimes), GTK_FILTER (filter)));
}

static void
ide_config_set_environ (IdeConfig          *self,
                        const char * const *environ)
{
  IdeEnvironment *env;

  g_assert (IDE_IS_CONFIG (self));

  env = ide_config_get_environment (self);
  ide_environment_set_environ (env, environ);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENVIRON]);
}

static gchar *
ide_config_repr (IdeObject *object)
{
  IdeConfig *self = (IdeConfig *)object;
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIG (self));

  return g_strdup_printf ("%s id=\"%s\" name=\"%s\" runtime=\"%s\"",
                          G_OBJECT_TYPE_NAME (self),
                          priv->id,
                          priv->display_name,
                          priv->runtime_id);
}

static void
ide_config_finalize (GObject *object)
{
  IdeConfig *self = (IdeConfig *)object;
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_clear_object (&priv->build_commands_dir);
  g_clear_object (&priv->environment);
  g_clear_object (&priv->runtime_environment);

  g_clear_pointer (&priv->build_commands, g_strfreev);
  g_clear_pointer (&priv->internal, g_hash_table_unref);
  g_clear_pointer (&priv->pipeline_args, g_hash_table_unref);
  g_clear_pointer (&priv->config_opts, g_free);
  g_clear_pointer (&priv->display_name, g_free);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->post_install_commands, g_strfreev);
  g_clear_pointer (&priv->prefix, g_free);
  g_clear_pointer (&priv->run_opts, g_free);
  g_clear_pointer (&priv->runtime_id, g_free);
  g_clear_pointer (&priv->app_id, g_free);
  g_clear_pointer (&priv->toolchain_id, g_free);
  g_clear_pointer (&priv->prepend_path, g_free);
  g_clear_pointer (&priv->append_path, g_free);

  G_OBJECT_CLASS (ide_config_parent_class)->finalize (object);
}

static void
ide_config_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeConfig *self = IDE_CONFIG (object);

  switch (prop_id)
    {
    case PROP_DESCRIPTION:
      g_value_take_string (value, ide_config_get_description (self));
      break;

    case PROP_CONFIG_OPTS:
      g_value_set_string (value, ide_config_get_config_opts (self));
      break;

    case PROP_BUILD_COMMANDS:
      g_value_set_boxed (value, ide_config_get_build_commands (self));
      break;

    case PROP_BUILD_COMMANDS_DIR:
      g_value_set_object (value, ide_config_get_build_commands_dir (self));
      break;

    case PROP_DEBUG:
      g_value_set_boolean (value, ide_config_get_debug (self));
      break;

    case PROP_DIRTY:
      g_value_set_boolean (value, ide_config_get_dirty (self));
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_config_get_display_name (self));
      break;

    case PROP_ENVIRON:
      g_value_set_boxed (value, ide_config_get_environ (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_config_get_id (self));
      break;

    case PROP_PARALLELISM:
      g_value_set_int (value, ide_config_get_parallelism (self));
      break;

    case PROP_READY:
      g_value_set_boolean (value, ide_config_get_ready (self));
      break;

    case PROP_POST_INSTALL_COMMANDS:
      g_value_set_boxed (value, ide_config_get_post_install_commands (self));
      break;

    case PROP_PREFIX:
      g_value_set_string (value, ide_config_get_prefix (self));
      break;

    case PROP_PREFIX_SET:
      g_value_set_boolean (value, ide_config_get_prefix_set (self));
      break;

    case PROP_RUNTIME:
      g_value_set_object (value, ide_config_get_runtime (self));
      break;

    case PROP_RUNTIME_ID:
      g_value_set_string (value, ide_config_get_runtime_id (self));
      break;

    case PROP_TOOLCHAIN:
      g_value_take_object (value, ide_config_get_toolchain (self));
      break;

    case PROP_TOOLCHAIN_ID:
      g_value_set_string (value, ide_config_get_toolchain_id (self));
      break;

    case PROP_RUN_OPTS:
      g_value_set_string (value, ide_config_get_run_opts (self));
      break;

    case PROP_APP_ID:
      g_value_set_string (value, ide_config_get_app_id (self));
      break;

    case PROP_PREPEND_PATH:
      g_value_set_string (value, ide_config_get_prepend_path (self));
      break;

    case PROP_APPEND_PATH:
      g_value_set_string (value, ide_config_get_append_path (self));
      break;

    case PROP_LOCALITY:
      g_value_set_flags (value, ide_config_get_locality (self));
      break;

    case PROP_SUPPORTED_RUNTIMES:
      g_value_take_object (value, ide_config_get_supported_runtimes (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_config_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeConfig *self = IDE_CONFIG (object);

  switch (prop_id)
    {
    case PROP_CONFIG_OPTS:
      ide_config_set_config_opts (self, g_value_get_string (value));
      break;

    case PROP_BUILD_COMMANDS:
      ide_config_set_build_commands (self, g_value_get_boxed (value));
      break;

    case PROP_BUILD_COMMANDS_DIR:
      ide_config_set_build_commands_dir (self, g_value_get_object (value));
      break;

    case PROP_DEBUG:
      ide_config_set_debug (self, g_value_get_boolean (value));
      break;

    case PROP_DIRTY:
      ide_config_set_dirty (self, g_value_get_boolean (value));
      break;

    case PROP_DISPLAY_NAME:
      ide_config_set_display_name (self, g_value_get_string (value));
      break;

    case PROP_ENVIRON:
      ide_config_set_environ (self, g_value_get_boxed (value));
      break;

    case PROP_ID:
      ide_config_set_id (self, g_value_get_string (value));
      break;

    case PROP_POST_INSTALL_COMMANDS:
      ide_config_set_post_install_commands (self, g_value_get_boxed (value));
      break;

    case PROP_PREFIX:
      ide_config_set_prefix (self, g_value_get_string (value));
      break;

    case PROP_PREFIX_SET:
      ide_config_set_prefix_set (self, g_value_get_boolean (value));
      break;

    case PROP_PARALLELISM:
      ide_config_set_parallelism (self, g_value_get_int (value));
      break;

    case PROP_RUNTIME:
      ide_config_set_runtime (self, g_value_get_object (value));
      break;

    case PROP_RUNTIME_ID:
      ide_config_set_runtime_id (self, g_value_get_string (value));
      break;

    case PROP_TOOLCHAIN:
      ide_config_set_toolchain (self, g_value_get_object (value));
      break;

    case PROP_TOOLCHAIN_ID:
      ide_config_set_toolchain_id (self, g_value_get_string (value));
      break;

    case PROP_RUN_OPTS:
      ide_config_set_run_opts (self, g_value_get_string (value));
      break;

    case PROP_APP_ID:
      ide_config_set_app_id (self, g_value_get_string (value));
      break;

    case PROP_PREPEND_PATH:
      ide_config_set_prepend_path (self, g_value_get_string (value));
      break;

    case PROP_APPEND_PATH:
      ide_config_set_append_path (self, g_value_get_string (value));
      break;

    case PROP_LOCALITY:
      ide_config_set_locality (self, g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_config_class_init (IdeConfigClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = ide_config_finalize;
  object_class->get_property = ide_config_get_property;
  object_class->set_property = ide_config_set_property;

  i_object_class->repr = ide_config_repr;

  klass->get_runtime = ide_config_real_get_runtime;
  klass->set_runtime = ide_config_real_set_runtime;
  klass->translate_file = ide_config_real_translate_file;

  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PREPEND_PATH] =
    g_param_spec_string ("prepend-path",
                         "Prepend Path",
                         "Prepend to PATH environment variable",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

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

  properties [PROP_BUILD_COMMANDS_DIR] =
    g_param_spec_object ("build-commands-dir",
                        "Build commands Dir",
                        "Directory to run build commands from",
                        G_TYPE_FILE,
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
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

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

  properties [PROP_PREFIX_SET] =
    g_param_spec_boolean ("prefix-set",
                          "Prefix Set",
                          "If Prefix is Set or not (meaning default)",
                          FALSE,
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

  properties [PROP_TOOLCHAIN] =
    g_param_spec_object ("toolchain",
                         "Toolchain",
                         "Toolchain",
                         IDE_TYPE_TOOLCHAIN,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TOOLCHAIN_ID] =
    g_param_spec_string ("toolchain-id",
                         "Toolchain Id",
                         "The identifier of the toolchain",
                         "default",
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

  properties[PROP_SUPPORTED_RUNTIMES] =
    g_param_spec_object ("supported-runtimes", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
ide_config_init (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  g_autoptr(IdeEnvironment) env = ide_environment_new ();
  g_autoptr(IdeEnvironment) rt_env = ide_environment_new ();

  priv->runtime_id = g_strdup ("host");
  priv->toolchain_id = g_strdup ("default");
  priv->debug = TRUE;
  priv->parallelism = -1;
  priv->locality = IDE_BUILD_LOCALITY_DEFAULT;
  priv->internal = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, _value_free);
  priv->pipeline_args = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_strfreev);

  ide_config_set_environment (self, env);
  ide_config_set_runtime_environment (self, rt_env);
}

/**
 * ide_config_get_app_id:
 * @self: An #IdeConfig
 *
 * Gets the application ID for the configuration.
 *
 * Returns: (transfer none) (nullable): A string.
 */
const gchar *
ide_config_get_app_id (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->app_id;
}

void
ide_config_set_app_id (IdeConfig   *self,
                       const gchar *app_id)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (priv->app_id != app_id)
    {
      g_free (priv->app_id);
      priv->app_id = g_strdup (app_id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_APP_ID]);
    }
}

const gchar *
ide_config_get_runtime_id (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->runtime_id;
}

void
ide_config_set_runtime_id (IdeConfig   *self,
                           const gchar *runtime_id)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (runtime_id == NULL)
    runtime_id = "host";

  if (g_set_str (&priv->runtime_id, runtime_id))
    {
      priv->runtime_ready = FALSE;

      ide_config_set_dirty (self, TRUE);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNTIME_ID]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNTIME]);

      if (priv->has_attached)
        {
          IdeRuntimeManager *runtime_manager;
          IdeContext *context;

          g_assert (IDE_IS_MAIN_THREAD ());

          context = ide_object_get_context (IDE_OBJECT (self));
          runtime_manager = ide_runtime_manager_from_context (context);
          ide_config_runtime_manager_items_changed (self, 0, 0, 0, runtime_manager);

          ide_config_emit_changed (self);
        }
    }
}

/**
 * ide_config_get_toolchain_id:
 * @self: An #IdeConfig
 *
 * Gets the toolchain id for the configuration.
 *
 * Returns: (transfer none) (nullable): The id of an #IdeToolchain or %NULL
 */
const gchar *
ide_config_get_toolchain_id (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->toolchain_id;
}

/**
 * ide_config_set_toolchain_id:
 * @self: An #IdeConfig
 * @toolchain_id: The id of an #IdeToolchain
 *
 * Sets the toolchain id for the configuration.
 */
void
ide_config_set_toolchain_id (IdeConfig   *self,
                             const gchar *toolchain_id)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (toolchain_id == NULL)
    toolchain_id = "default";

  if (g_set_str (&priv->toolchain_id,toolchain_id))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TOOLCHAIN_ID]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TOOLCHAIN]);

      ide_config_set_dirty (self, TRUE);
    }
}

/**
 * ide_config_get_runtime:
 * @self: An #IdeConfig
 *
 * Gets the runtime for the configuration.
 *
 * Returns: (transfer none) (nullable): An #IdeRuntime
 */
IdeRuntime *
ide_config_get_runtime (IdeConfig *self)
{
  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return IDE_CONFIG_GET_CLASS (self)->get_runtime (self);
}

void
ide_config_set_runtime (IdeConfig  *self,
                        IdeRuntime *runtime)
{
  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (!runtime || IDE_IS_RUNTIME (runtime));

  IDE_CONFIG_GET_CLASS (self)->set_runtime (self, runtime);
}

/**
 * ide_config_get_toolchain:
 * @self: An #IdeConfig
 *
 * Gets the toolchain for the configuration.
 *
 * Returns: (transfer full) (nullable): An #IdeToolchain
 */
IdeToolchain *
ide_config_get_toolchain (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  if (priv->toolchain_id != NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeToolchainManager *toolchain_manager = ide_toolchain_manager_from_context (context);
      g_autoptr (IdeToolchain) toolchain = ide_toolchain_manager_get_toolchain (toolchain_manager, priv->toolchain_id);

      if (toolchain != NULL)
        return g_steal_pointer (&toolchain);
    }

  return NULL;
}

/**
 * ide_config_set_toolchain:
 * @self: An #IdeConfig
 * @toolchain: (nullable): An #IdeToolchain or %NULL to use the default one
 *
 * Sets the toolchain for the configuration.
 */
void
ide_config_set_toolchain (IdeConfig    *self,
                          IdeToolchain *toolchain)
{
  const gchar *toolchain_id = "default";

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (!toolchain || IDE_IS_TOOLCHAIN (toolchain));

  if (toolchain != NULL)
    toolchain_id = ide_toolchain_get_id (toolchain);

  ide_config_set_toolchain_id (self, toolchain_id);
}

/**
 * ide_config_get_environ:
 * @self: An #IdeConfig
 *
 * Gets the environment to use when spawning processes.
 *
 * Returns: (transfer full): An array of key=value environment variables.
 */
gchar **
ide_config_get_environ (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return ide_environment_get_environ (priv->environment);
}

const gchar *
ide_config_getenv (IdeConfig   *self,
                   const gchar *key)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return ide_environment_getenv (priv->environment, key);
}

void
ide_config_setenv (IdeConfig   *self,
                   const gchar *key,
                   const gchar *value)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (key != NULL);

  ide_environment_setenv (priv->environment, key, value);
}

const gchar *
ide_config_get_id (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->id;
}

const gchar *
ide_config_get_prefix (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->prefix;
}

void
ide_config_set_prefix (IdeConfig   *self,
                       const gchar *prefix)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (g_set_str (&priv->prefix, prefix))
    {
      priv->prefix_set = TRUE;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PREFIX]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PREFIX_SET]);
      ide_config_set_dirty (self, TRUE);
    }
}

gint
ide_config_get_parallelism (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), -1);

  if (priv->parallelism == -1)
    {
      g_autoptr(GSettings) settings = g_settings_new ("org.gnome.builder.build");

      return g_settings_get_int (settings, "parallel");
    }

  return priv->parallelism;
}

void
ide_config_set_parallelism (IdeConfig *self,
                            gint       parallelism)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (parallelism >= -1);

  if (parallelism != priv->parallelism)
    {
      priv->parallelism = parallelism;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PARALLELISM]);
    }
}

gboolean
ide_config_get_debug (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), FALSE);

  return priv->debug;
}

void
ide_config_set_debug (IdeConfig *self,
                      gboolean   debug)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  debug = !!debug;

  if (debug != priv->debug)
    {
      priv->debug = debug;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUG]);
      ide_config_set_dirty (self, TRUE);
    }
}

const gchar *
ide_config_get_display_name (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->display_name;
}

void
ide_config_set_display_name (IdeConfig   *self,
                             const gchar *display_name)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (g_set_str (&priv->display_name, display_name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
      ide_config_set_dirty (self, TRUE);
    }
}

gboolean
ide_config_get_dirty (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), FALSE);

  return priv->dirty;
}

void
ide_config_set_dirty (IdeConfig *self,
                      gboolean   dirty)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (priv->block_changed)
    IDE_EXIT;

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
      ide_config_emit_changed (self);
    }

  IDE_EXIT;
}

/**
 * ide_config_get_environment:
 *
 * Returns: (transfer none): An #IdeEnvironment.
 */
IdeEnvironment *
ide_config_get_environment (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->environment;
}

void
ide_config_set_environment (IdeConfig      *self,
                            IdeEnvironment *environment)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (!environment || IDE_IS_ENVIRONMENT (environment));

  if (priv->environment != environment)
    {
      if (priv->environment != NULL)
        {
          g_signal_handlers_disconnect_by_func (priv->environment,
                                                G_CALLBACK (ide_config_environment_changed),
                                                self);
          g_clear_object (&priv->environment);
        }

      if (environment != NULL)
        {
          priv->environment = g_object_ref (environment);
          g_signal_connect_object (priv->environment,
                                   "changed",
                                   G_CALLBACK (ide_config_environment_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENVIRON]);
    }
}

/**
 * ide_config_get_runtime_environment:
 *
 * Returns: (transfer none): An #IdeEnvironment.
 */
IdeEnvironment *
ide_config_get_runtime_environment (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->runtime_environment;
}

void
ide_config_set_runtime_environment (IdeConfig      *self,
                                    IdeEnvironment *environment)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (!environment || IDE_IS_ENVIRONMENT (environment));

  if (priv->runtime_environment != environment)
    {
      if (priv->runtime_environment != NULL)
        {
          g_signal_handlers_disconnect_by_func (priv->runtime_environment,
                                                G_CALLBACK (ide_config_runtime_environment_changed),
                                                self);
          g_clear_object (&priv->runtime_environment);
        }

      if (environment != NULL)
        {
          priv->runtime_environment = g_object_ref (environment);
          g_signal_connect_object (priv->runtime_environment,
                                   "changed",
                                   G_CALLBACK (ide_config_runtime_environment_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
        }
    }
}

const gchar *
ide_config_get_config_opts (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->config_opts;
}

void
ide_config_set_config_opts (IdeConfig   *self,
                            const gchar *config_opts)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (g_set_str (&priv->config_opts, config_opts))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONFIG_OPTS]);
      ide_config_set_dirty (self, TRUE);
    }
}

const gchar * const *
ide_config_get_build_commands (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return (const gchar * const *)priv->build_commands;
}

void
ide_config_set_build_commands (IdeConfig           *self,
                               const gchar * const *build_commands)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (ide_set_strv (&priv->build_commands, build_commands))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUILD_COMMANDS]);
      ide_config_set_dirty (self, TRUE);
    }
}

const gchar * const *
ide_config_get_post_install_commands (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return (const gchar * const *)priv->post_install_commands;
}

void
ide_config_set_post_install_commands (IdeConfig           *self,
                                      const gchar * const *post_install_commands)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (ide_set_strv (&priv->post_install_commands, post_install_commands))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_POST_INSTALL_COMMANDS]);
      ide_config_set_dirty (self, TRUE);
    }
}

/**
 * ide_config_get_sequence:
 * @self: An #IdeConfig
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
ide_config_get_sequence (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), 0);

  return priv->sequence;
}

static GValue *
ide_config_reset_internal_value (IdeConfig   *self,
                                 const gchar *key,
                                 GType        type)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  GValue *v;

  g_assert (IDE_IS_CONFIG (self));
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
ide_config_get_internal_string (IdeConfig   *self,
                                const gchar *key)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS_STRING (v))
    return g_value_get_string (v);

  return NULL;
}

void
ide_config_set_internal_string (IdeConfig   *self,
                                const gchar *key,
                                const gchar *value)
{
  GValue *v;

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (key != NULL);

  v = ide_config_reset_internal_value (self, key, G_TYPE_STRING);
  g_value_set_string (v, value);
}

const gchar * const *
ide_config_get_internal_strv (IdeConfig   *self,
                              const gchar *key)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS (v, G_TYPE_STRV))
    return g_value_get_boxed (v);

  return NULL;
}

void
ide_config_set_internal_strv (IdeConfig           *self,
                              const gchar         *key,
                              const gchar * const *value)
{
  GValue *v;

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (key != NULL);

  v = ide_config_reset_internal_value (self, key, G_TYPE_STRV);
  g_value_set_boxed (v, value);
}

gboolean
ide_config_get_internal_boolean (IdeConfig   *self,
                                 const gchar *key)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS_BOOLEAN (v))
    return g_value_get_boolean (v);

  return FALSE;
}

void
ide_config_set_internal_boolean (IdeConfig   *self,
                                 const gchar *key,
                                 gboolean     value)
{
  GValue *v;

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (key != NULL);

  v = ide_config_reset_internal_value (self, key, G_TYPE_BOOLEAN);
  g_value_set_boolean (v, value);
}

gint
ide_config_get_internal_int (IdeConfig   *self,
                             const gchar *key)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIG (self), -1);
  g_return_val_if_fail (key != NULL, -1);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS_INT (v))
    return g_value_get_int (v);

  return 0;
}

void
ide_config_set_internal_int (IdeConfig   *self,
                             const gchar *key,
                             gint         value)
{
  GValue *v;

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (key != NULL);

  v = ide_config_reset_internal_value (self, key, G_TYPE_INT);
  g_value_set_int (v, value);
}

gint64
ide_config_get_internal_int64 (IdeConfig *self,
                                      const gchar      *key)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIG (self), -1);
  g_return_val_if_fail (key != NULL, -1);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS_INT64 (v))
    return g_value_get_int64 (v);

  return 0;
}

void
ide_config_set_internal_int64 (IdeConfig   *self,
                               const gchar *key,
                               gint64       value)
{
  GValue *v;

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (key != NULL);

  v = ide_config_reset_internal_value (self, key, G_TYPE_INT64);
  g_value_set_int64 (v, value);
}

/**
 * ide_config_get_internal_object:
 * @self: An #IdeConfig
 * @key: The key to get
 *
 * Gets the value associated with @key if it is a #GObject.
 *
 * Returns: (nullable) (transfer none) (type GObject.Object): a #GObject or %NULL.
 */
gpointer
ide_config_get_internal_object (IdeConfig   *self,
                                const gchar *key)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  const GValue *v;

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  v = g_hash_table_lookup (priv->internal, key);

  if (v != NULL && G_VALUE_HOLDS_OBJECT (v))
    return g_value_get_object (v);

  return NULL;
}

/**
 * ide_config_set_internal_object:
 * @self: an #IdeConfig
 * @key: the key to set
 * @instance: (type GObject.Object) (nullable): a #GObject or %NULL
 *
 * Sets the value for @key to @instance.
 */
void
ide_config_set_internal_object (IdeConfig   *self,
                                const gchar *key,
                                gpointer     instance)
{
  GValue *v;
  GType type;

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (key != NULL);

  if (instance != NULL)
    type = G_OBJECT_TYPE (instance);
  else
    type = G_TYPE_OBJECT;

  v = ide_config_reset_internal_value (self, key, type);
  g_value_set_object (v, instance);
}

/**
 * ide_config_get_ready:
 * @self: An #IdeConfig
 *
 * Determines if the configuration is ready for use.
 *
 * Returns: %TRUE if the configuration is ready for use.
 */
gboolean
ide_config_get_ready (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), FALSE);

  return priv->runtime_ready;
}

gboolean
ide_config_supports_runtime (IdeConfig  *self,
                             IdeRuntime *runtime)
{
  gboolean ret = TRUE;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (IDE_IS_RUNTIME (runtime), FALSE);

  if (IDE_CONFIG_GET_CLASS (self)->supports_runtime)
    ret = IDE_CONFIG_GET_CLASS (self)->supports_runtime (self, runtime);

  IDE_RETURN (ret);
}

/**
 * ide_config_get_run_opts:
 * @self: a #IdeConfig
 *
 * Gets the command line options to use when running the target application.
 * The result should be parsed with g_shell_parse_argv() to convert the run
 * options to an array suitable for use in argv.
 *
 * Returns: (transfer none) (nullable): A string containing the run options
 *   or %NULL if none have been set.
 */
const gchar *
ide_config_get_run_opts (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->run_opts;
}

/**
 * ide_config_set_run_opts:
 * @self: a #IdeConfig
 * @run_opts: (nullable): the run options for the target application
 *
 * Sets the run options to use when running the target application.
 * See ide_config_get_run_opts() for more information.
 */
void
ide_config_set_run_opts (IdeConfig   *self,
                         const gchar *run_opts)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (g_set_str (&priv->run_opts, run_opts))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUN_OPTS]);
      ide_config_set_dirty (self, TRUE);
    }
}

const gchar *
ide_config_get_prepend_path (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->prepend_path;
}

void
ide_config_set_prepend_path (IdeConfig   *self,
                             const gchar *prepend_path)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (g_set_str (&priv->prepend_path, prepend_path))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PREPEND_PATH]);
    }
}

const gchar *
ide_config_get_append_path (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->append_path;
}

void
ide_config_set_append_path (IdeConfig   *self,
                            const gchar *append_path)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  if (g_set_str (&priv->append_path, append_path))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_APPEND_PATH]);
    }
}

void
ide_config_apply_path (IdeConfig             *self,
                       IdeSubprocessLauncher *launcher)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  if (priv->prepend_path != NULL)
    ide_subprocess_launcher_prepend_path (launcher, priv->prepend_path);

  if (priv->append_path != NULL)
    ide_subprocess_launcher_append_path (launcher, priv->append_path);
}

IdeBuildLocality
ide_config_get_locality (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), 0);

  return priv->locality;
}

void
ide_config_set_locality (IdeConfig        *self,
                         IdeBuildLocality  locality)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (locality > 0);
  g_return_if_fail (locality <= IDE_BUILD_LOCALITY_DEFAULT);

  if (priv->locality != locality)
    {
      priv->locality = locality;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOCALITY]);
      ide_config_set_dirty (self, TRUE);
    }
}

/**
 * ide_config_get_build_commands_dir:
 * @self: a #IdeConfig
 *
 * Returns: (transfer none) (nullable): a #GFile or %NULL
 */
GFile *
ide_config_get_build_commands_dir (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  return priv->build_commands_dir;
}

void
ide_config_set_build_commands_dir (IdeConfig *self,
                                   GFile     *build_commands_dir)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (!build_commands_dir || G_IS_FILE (build_commands_dir));

  if (g_set_object (&priv->build_commands_dir, build_commands_dir))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUILD_COMMANDS_DIR]);
}

void
_ide_config_attach (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  IdeRuntimeManager *runtime_manager;
  IdeContext *context;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (priv->has_attached == FALSE);

  priv->has_attached = TRUE;

  /*
   * We don't start monitoring changed events until we've gotten back
   * to the main loop (in case of threaded loaders) which happens from
   * the point where the configuration is added to the config manager.
   */

  if (!(context = ide_object_get_context (IDE_OBJECT (self))))
    {
      g_critical ("Attempt to register configuration without a context");
      return;
    }

  runtime_manager = ide_runtime_manager_from_context (context);

  g_signal_connect_object (runtime_manager,
                           "items-changed",
                           G_CALLBACK (ide_config_runtime_manager_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  /* Update the runtime and potentially set prefix, but do not emit changed */
  ide_config_block_changed (self);
  ide_config_runtime_manager_items_changed (self, 0, 0, 0, runtime_manager);
  ide_config_unblock_changed (self);
}

gboolean
ide_config_get_prefix_set (IdeConfig *self)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_CONFIG (self), FALSE);

  return priv->prefix_set;
}

void
ide_config_set_prefix_set (IdeConfig *self,
                           gboolean   prefix_set)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  prefix_set = !!prefix_set;

  if (prefix_set != priv->prefix_set)
    {
      priv->prefix_set = prefix_set;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PREFIX_SET]);
    }
}

/**
 * ide_config_get_extensions:
 * @self: a #IdeConfig
 *
 * Gets the known SDK extensions that will be used when building the project.
 * Implementing this in your configuration backend allows plugins to know if
 * additional binaries will be available to the build system.
 *
 * Returns: (not nullable) (transfer full) (element-type Ide.Runtime): an array
 *   of #IdeRuntime for the runtime extensions for the configuration.
 */
GPtrArray *
ide_config_get_extensions (IdeConfig *self)
{
  GPtrArray *ret = NULL;

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  if (IDE_CONFIG_GET_CLASS (self)->get_extensions)
   ret = IDE_CONFIG_GET_CLASS (self)->get_extensions (self);

  if (ret == NULL)
    ret = g_ptr_array_new ();

  return g_steal_pointer (&ret);
}

const gchar * const *
ide_config_get_args_for_phase (IdeConfig        *self,
                               IdePipelinePhase  phase)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);
  const gchar * const *args;

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  args = g_hash_table_lookup (priv->pipeline_args, GINT_TO_POINTER (phase));
  return args;
}

void
ide_config_set_args_for_phase (IdeConfig           *self,
                               IdePipelinePhase     phase,
                               const gchar * const *args)
{
  IdeConfigPrivate *priv = ide_config_get_instance_private (self);

  g_return_if_fail (IDE_IS_CONFIG (self));

  g_hash_table_insert (priv->pipeline_args, GINT_TO_POINTER (phase), g_strdupv ((gchar **)args));
}

/**
 * ide_config_get_description:
 * @self: a #IdeConfig
 *
 * Describes the type of config this is.
 *
 * Examples might include ".buildconfig" or "Flatpak".
 *
 * Returns: (transfer full): a string describing the configuration.
 */
char *
ide_config_get_description (IdeConfig *self)
{
  char *ret = NULL;

  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);

  if (IDE_CONFIG_GET_CLASS (self)->get_description)
    ret = IDE_CONFIG_GET_CLASS (self)->get_description (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return ret;
}

/**
 * ide_config_translate_file:
 * @self: a #IdeConfig
 *
 * Requests translation of the file path to one available in the
 * current process. That might mean translating to a path that
 * allows access outside Builder's sandbox such as using
 * /var/run/host or depoy-directories of OSTree commits.
 *
 * Returns: (transfer full) (nullable): a #GFile or %NULL
 */
GFile *
ide_config_translate_file (IdeConfig *self,
                           GFile     *file)
{
  g_return_val_if_fail (IDE_IS_CONFIG (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  return IDE_CONFIG_GET_CLASS (self)->translate_file (self, file);
}

/* Try to avoid adding extra '' or "" when replacing
 * strings to be joined into a new config-opts.
 */
static char *
quote_arg (const char *in)
{
  g_autofree char *quoted = g_shell_quote (in);
  gsize len = strlen (quoted);

  if (len < 2)
    return g_strdup (in);

  for (const char *c = in; *c; c = g_utf8_next_char (c))
    {
      gunichar ch = g_utf8_get_char (c);

      switch (ch)
        {
        case '\t':
        case '\r':
        case '\n':
        case ' ':
        case '\"':
        case '\'':
          return g_steal_pointer (&quoted);

        default:
          if (g_unichar_isspace (ch))
            return g_steal_pointer (&quoted);
          break;
        }
    }

  return g_strdup (in);
}

gboolean
_ide_config_has_config_opt (IdeConfig  *self,
                            const char *param)
{
  const char *config_opts;

  g_return_val_if_fail (IDE_IS_CONFIG (self), FALSE);
  g_return_val_if_fail (param != NULL, FALSE);

  if ((config_opts = ide_config_get_config_opts (self)) && !ide_str_empty0 (config_opts))
    {
      g_auto(GStrv) args = NULL;
      int argc;

      if (!g_shell_parse_argv (config_opts, &argc, &args, NULL))
        return FALSE;

      for (int i = 0; i < argc; i++)
        {
          if (g_str_equal (param, args[i]) ||
              (g_str_has_prefix (args[i], param) &&
               args[i][strlen(param)] == '='))
            return TRUE;
        }
    }

  return FALSE;
}

void
ide_config_replace_config_opt (IdeConfig  *self,
                               const char *param,
                               const char *value)
{
  g_autoptr(GStrvBuilder) builder = NULL;
  g_autoptr(GString) strv = NULL;
  g_auto(GStrv) built = NULL;
  g_auto(GStrv) args = NULL;
  const char *config_opts;
  gboolean found = FALSE;
  gsize len;
  int argc = 0;

  g_return_if_fail (IDE_IS_CONFIG (self));
  g_return_if_fail (param != NULL);
  g_return_if_fail (value != NULL);

  if ((config_opts = ide_config_get_config_opts (self)) && !ide_str_empty0 (config_opts))
    {
      if (!g_shell_parse_argv (config_opts, &argc, &args, NULL))
        return;
    }

  len = strlen (param);
  builder = g_strv_builder_new ();

  for (guint i = 0; i < argc; i++)
    {
      if (g_str_equal (args[i], param))
        {
          g_strv_builder_add (builder, param);
          g_strv_builder_add (builder, value);
          i++;
          found = TRUE;
        }
      else if (g_str_has_prefix (args[i], param) && args[i][len] == '=')
        {
          g_autofree char *full = g_strdup_printf ("%s=%s", param, value);
          g_strv_builder_add (builder, full);
          found = TRUE;
        }
      else
        {
          g_strv_builder_add (builder, args[i]);
        }
    }

  if (!found)
    {
      g_autofree char *full = g_strdup_printf ("%s=%s", param, value);
      g_strv_builder_add (builder, full);
    }

  built = g_strv_builder_end (builder);
  strv = g_string_new (NULL);

  for (guint i = 0; built[i]; i++)
    {
      g_autofree char *quoted = quote_arg (built[i]);

      if (i > 0)
        g_string_append_c (strv, ' ');

      g_string_append (strv, quoted);
    }

  ide_config_set_config_opts (self, strv->str);
}
