/* ide-toolchain.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#define G_LOG_DOMAIN "ide-toolchain"

#include "config.h"

#include "ide-debug.h"
#include "ide-context.h"

#include "toolchain/ide-toolchain.h"
#include "util/ide-posix.h"


typedef struct
{
  gchar *id;
  gchar *host_architecture;
  gchar *host_kernel;
  gchar *host_system;
  GHashTable *compilers;
  gchar *archiver;
  gchar *strip;
  gchar *pkg_config;
  gchar *exe_wrapper;
} IdeToolchainPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeToolchain, ide_toolchain, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  PROP_HOST_ARCHITECTURE,
  PROP_HOST_KERNEL,
  PROP_HOST_SYSTEM,
  PROP_COMPILERS,
  PROP_ARCHIVER,
  PROP_STRIP,
  PROP_PKG_CONFIG,
  PROP_EXE_WRAPPER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

IdeToolchain *
ide_toolchain_new (IdeContext   *context,
                   const gchar  *id)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  return g_object_new (IDE_TYPE_TOOLCHAIN,
                       "context", context,
                       "id", id,
                       NULL);
}

/**
 * ide_toolchain_get_id:
 * @self: an #IdeToolchain
 *
 * Gets the internal identifier of the toolchain
 *
 * Returns: (transfer none): the unique identifier.
 *
 * Since: 3.30
 */
const gchar *
ide_toolchain_get_id (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->id;
}

void
ide_toolchain_set_id (IdeToolchain  *self,
                      const gchar   *id)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));
  g_return_if_fail (id != NULL);

  if (g_strcmp0 (id, priv->id) != 0)
    {
      g_free (priv->id);
      priv->id = g_strdup (id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

/**
 * ide_toolchain_get_host_architecture:
 * @self: an #IdeToolchain
 *
 * Gets the host architecture of the toolchain
 *
 * Returns: (transfer none): The host architecture.
 *
 * Since: 3.30
 */
const gchar *
ide_toolchain_get_host_architecture (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->host_architecture;
}

/**
 * ide_toolchain_set_host_architecture:
 * @self: an #IdeToolchain
 * @host_architecture: the host architecture of the toolchain
 *
 * Sets the host architecture of the toolchain
 *
 * Since: 3.30
 */
void
ide_toolchain_set_host_architecture (IdeToolchain  *self,
                                     const gchar   *host_architecture)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));

  if (g_strcmp0 (host_architecture, priv->host_architecture) != 0)
    {
      g_free (priv->host_architecture);
      priv->host_architecture = g_strdup (host_architecture);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HOST_ARCHITECTURE]);
    }
}

/**
 * ide_toolchain_get_host_kernel:
 * @self: an #IdeToolchain
 *
 * Gets the host architecture of the toolchain
 *
 * Returns: (nullable): a string describing the host kernel, or %NULL
 *
 * Since: 3.30
 */
const gchar *
ide_toolchain_get_host_kernel (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->host_kernel;
}

/**
 * ide_toolchain_set_host_kernel:
 * @self: an #IdeToolchain
 * @host_kernel: the host architecture of the toolchain
 *
 * Sets the host kernel of the toolchain
 *
 * Since: 3.30
 */
void
ide_toolchain_set_host_kernel (IdeToolchain  *self,
                               const gchar   *host_kernel)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));

  if (g_strcmp0 (host_kernel, priv->host_kernel) != 0)
    {
      g_free (priv->host_kernel);
      priv->host_kernel = g_strdup (host_kernel);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HOST_KERNEL]);
    }
}

/**
 * ide_toolchain_get_host_system:
 * @self: an #IdeToolchain
 *
 * Gets the system portion of the host tripet that the pipeline is
 * building for, once that has been discovered from the device.
 *
 * For Linux based devices, this will generally be "gnu".
 *
 * Returns: (nullable): The host system
 *
 * Since: 3.30
 */
const gchar *
ide_toolchain_get_host_system (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->host_system;
}

/**
 * ide_toolchain_set_host_system:
 * @self: an #IdeToolchain
 * @host_system: the host system of the toolchain
 *
 * Sets the host system of the toolchain
 *
 * Since: 3.30
 */
void
ide_toolchain_set_host_system (IdeToolchain  *self,
                               const gchar   *host_system)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));

  if (g_strcmp0 (host_system, priv->host_system) != 0)
    {
      g_free (priv->host_system);
      priv->host_architecture = g_strdup (host_system);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HOST_SYSTEM]);
    }
}

/**
 * ide_toolchain_get_host_system_type:
 * @self: an #IdeToolchain
 *
 * Gets the combination of arch-kernel-system, sometimes referred to as
 * the "host triplet".
 *
 * For Linux based devices, this will generally be something like
 * "x86_64-linux-gnu".
 *
 * Returns: (transfer full): The host system.type of the toolchain
 *
 * Since: 3.30
 */
gchar *
ide_toolchain_get_host_system_type (IdeToolchain *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return ide_create_host_triplet (priv->host_architecture, priv->host_kernel, priv->host_system);
}

/**
 * ide_toolchain_get_compilers:
 * @self: an #IdeToolchain
 *
 * Gets the path of the compiler executable
 *
 * Returns: (transfer none) (element-type utf8 utf8): A #GHashTable containing a the entries of
 * language and compiler paths.
 *
 * Since: 3.30
 */
GHashTable *
ide_toolchain_get_compilers (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->compilers;
}

/**
 * ide_toolchain_get_compiler:
 * @self: an #IdeToolchain
 * @language: the name of the language supported by this compiler
 *
 * Gets the path of the compiler executable
 *
 * Returns: (nullable) (transfer none): A path or %NULL.
 *
 * Since: 3.30
 */
const gchar *
ide_toolchain_get_compiler (IdeToolchain  *self,
                            const gchar   *language)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return g_hash_table_lookup (priv->compilers, language);
}

/**
 * ide_toolchain_set_compiler:
 * @self: an #IdeToolchain
 * @language: the name of the language supported by this compiler
 * @path: (nullable): the path to the archiver executable
 *
 * Sets the path of the compiler executable
 * If the @path is %NULL then the language row will simply be removed from the #GHashTable
 *
 * Since: 3.30
 */
void
ide_toolchain_set_compiler (IdeToolchain  *self,
                            const gchar   *language,
                            const gchar   *path)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);
  const gchar *current_path = ide_toolchain_get_compiler (self, language);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));

  if (g_strcmp0 (path, current_path) != 0)
    {
      if (path == NULL)
        g_hash_table_remove (priv->compilers, language);
      else
        g_hash_table_insert (priv->compilers, g_strdup (language), g_strdup (path));

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPILERS]);
    }
}

/**
 * ide_toolchain_get_archiver:
 * @self: an #IdeToolchain
 *
 * Gets the path of the archiver executable
 *
 * Returns: (nullable) (transfer none): A path or %NULL.
 *
 * Since: 3.30
 */
const gchar *
ide_toolchain_get_archiver (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->archiver;
}

/**
 * ide_toolchain_set_archiver:
 * @self: an #IdeToolchain
 * @path: (nullable): the path to the archiver executable
 *
 * Sets the path of the archiver executable
 *
 * Since: 3.30
 */
void
ide_toolchain_set_archiver (IdeToolchain  *self,
                            const gchar   *path)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));

  if (g_strcmp0 (path, priv->archiver) != 0)
    {
      g_free (priv->archiver);
      priv->archiver = g_strdup (path);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ARCHIVER]);
    }
}

/**
 * ide_toolchain_get_strip:
 * @self: an #IdeToolchain
 *
 * Gets the path of the strip executable
 *
 * Returns: (nullable) (transfer none): A path or %NULL.
 *
 * Since: 3.30
 */
const gchar *
ide_toolchain_get_strip (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->strip;
}

/**
 * ide_toolchain_set_strip:
 * @self: an #IdeToolchain
 * @path: (nullable): the path to the strip executable
 *
 * Sets the path of the strip executable
 *
 * Since: 3.30
 */
void
ide_toolchain_set_strip (IdeToolchain  *self,
                         const gchar   *path)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));

  if (g_strcmp0 (path, priv->strip) != 0)
    {
      g_free (priv->strip);
      priv->strip = g_strdup (path);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STRIP]);
    }
}

/**
 * ide_toolchain_get_pkg_config:
 * @self: an #IdeToolchain
 *
 * Gets the path of the pkg-config executable
 *
 * Returns: (nullable) (transfer none): A path or %NULL.
 *
 * Since: 3.30
 */
const gchar *
ide_toolchain_get_pkg_config (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->pkg_config;
}

/**
 * ide_toolchain_set_pkg_config:
 * @self: an #IdeToolchain
 * @path: (nullable): the path to the pkg-config executable
 *
 * Sets the path of the pkg-config executable
 *
 * Since: 3.30
 */
void
ide_toolchain_set_pkg_config (IdeToolchain  *self,
                              const gchar   *path)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));

  if (g_strcmp0 (path, priv->pkg_config) != 0)
    {
      g_free (priv->pkg_config);
      priv->pkg_config = g_strdup (path);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PKG_CONFIG]);
    }
}

/**
 * ide_toolchain_get_exe_wrapper:
 * @self: an #IdeToolchain
 *
 * Gets the path of the wrapper to use when running the compiled executables
 *
 * Returns: (nullable) (transfer none): A path or %NULL.
 *
 * Since: 3.30
 */
const gchar *
ide_toolchain_get_exe_wrapper (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->exe_wrapper;
}

/**
 * ide_toolchain_set_exe_wrapper:
 * @self: an #IdeToolchain
 * @path: (nullable): the path of the executable wrapper
 *
 * Sets the path of the wrapper to use when running the compiled executables
 *
 * Since: 3.30
 */
void
ide_toolchain_set_exe_wrapper (IdeToolchain  *self,
                               const gchar   *path)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));

  if (g_strcmp0 (path, priv->exe_wrapper) != 0)
    {
      g_free (priv->exe_wrapper);
      priv->exe_wrapper = g_strdup (path);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXE_WRAPPER]);
    }
}

static void
ide_toolchain_finalize (GObject *object)
{
  IdeToolchain *self = (IdeToolchain *)object;
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->host_architecture, g_free);
  g_clear_pointer (&priv->host_kernel, g_free);
  g_clear_pointer (&priv->host_system, g_free);
  g_clear_pointer (&priv->compilers, g_hash_table_unref);
  g_clear_pointer (&priv->archiver, g_free);
  g_clear_pointer (&priv->strip, g_free);
  g_clear_pointer (&priv->pkg_config, g_free);
  g_clear_pointer (&priv->exe_wrapper, g_free);

  G_OBJECT_CLASS (ide_toolchain_parent_class)->finalize (object);
}

static void
ide_toolchain_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeToolchain *self = IDE_TOOLCHAIN (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_toolchain_get_id (self));
      break;
    case PROP_HOST_ARCHITECTURE:
      g_value_set_boxed (value, ide_toolchain_get_host_architecture (self));
      break;
    case PROP_HOST_KERNEL:
      g_value_set_boxed (value, ide_toolchain_get_host_kernel (self));
      break;
    case PROP_HOST_SYSTEM:
      g_value_set_boxed (value, ide_toolchain_get_host_system (self));
      break;
    case PROP_COMPILERS:
      g_value_set_boxed (value, ide_toolchain_get_compilers (self));
      break;
    case PROP_ARCHIVER:
      g_value_set_string (value, ide_toolchain_get_archiver (self));
      break;
    case PROP_STRIP:
      g_value_set_string (value, ide_toolchain_get_strip (self));
      break;
    case PROP_PKG_CONFIG:
      g_value_set_string (value, ide_toolchain_get_pkg_config (self));
      break;
    case PROP_EXE_WRAPPER:
      g_value_set_string (value, ide_toolchain_get_exe_wrapper (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_toolchain_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeToolchain *self = IDE_TOOLCHAIN (object);

  switch (prop_id)
    {
    case PROP_ID:
      ide_toolchain_set_id (self, g_value_get_string (value));
      break;
    case PROP_HOST_ARCHITECTURE:
      ide_toolchain_set_host_architecture (self, g_value_get_string (value));
      break;
    case PROP_HOST_KERNEL:
      ide_toolchain_set_host_kernel (self, g_value_get_string (value));
      break;
    case PROP_HOST_SYSTEM:
      ide_toolchain_set_host_system (self, g_value_get_string (value));
      break;
    case PROP_ARCHIVER:
      ide_toolchain_set_archiver (self, g_value_get_string (value));
      break;
    case PROP_STRIP:
      ide_toolchain_set_strip (self, g_value_get_string (value));
      break;
    case PROP_PKG_CONFIG:
      ide_toolchain_set_pkg_config (self, g_value_get_string (value));
      break;
    case PROP_EXE_WRAPPER:
      ide_toolchain_set_exe_wrapper (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_toolchain_class_init (IdeToolchainClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_toolchain_finalize;
  object_class->get_property = ide_toolchain_get_property;
  object_class->set_property = ide_toolchain_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The toolchain identifier",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_HOST_ARCHITECTURE] =
    g_param_spec_string ("host-architecture",
                         "Host architecture",
                         "The architecture of the machine on which the compiled binary will run, such as x86_64",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_HOST_KERNEL] =
    g_param_spec_string ("host-kernel",
                         "Host kernel",
                         "The operating system kernel of the machine on which the compiled binary will run, such as Linux",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_HOST_SYSTEM] =
    g_param_spec_string ("host-system",
                         "Host system",
                         "The system name of the machine on which the compiled binary will run, such as 'gnu'",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_COMPILERS] =
    g_param_spec_boxed ("compilers",
                         "Compilers",
                         "The table of languages and compiler executables",
                         G_TYPE_HASH_TABLE,
                         (G_PARAM_READABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ARCHIVER] =
    g_param_spec_string ("archiver",
                         "Archiver",
                         "The path to the archiver executable",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_STRIP] =
    g_param_spec_string ("strip",
                         "Strip",
                         "The path to the strip executable",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_PKG_CONFIG] =
    g_param_spec_string ("pkg-config",
                         "PkgConfig",
                         "The path to the pkg-config executable",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_EXE_WRAPPER] =
    g_param_spec_string ("exe-wrapper",
                         "Exe Wrapper",
                         "The path of the wrapper to use when running the compiled executables",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_toolchain_init (IdeToolchain *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);
  priv->compilers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  priv->host_architecture = ide_get_system_arch ();
}
