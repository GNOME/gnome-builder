/* ide-toolchain.c
 *
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
 *
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-toolchain"

#include "config.h"

#include "ide-toolchain.h"
#include "ide-triplet.h"

typedef struct
{
  gchar *id;
  gchar *display_name;
  IdeTriplet *host_triplet;
} IdeToolchainPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeToolchain, ide_toolchain, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  PROP_DISPLAY_NAME,
  PROP_HOST_TRIPLET,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ide_toolchain_get_id:
 * @self: an #IdeToolchain
 *
 * Gets the internal identifier of the toolchain
 *
 * Returns: (transfer none): the unique identifier.
 */
const gchar *
ide_toolchain_get_id (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->id;
}


/**
 * ide_toolchain_set_id:
 * @self: an #IdeToolchain
 * @id: the unique identifier
 *
 * Sets the internal identifier of the toolchain
 */
void
ide_toolchain_set_id (IdeToolchain  *self,
                      const gchar   *id)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));
  g_return_if_fail (id != NULL);

  if (g_set_str (&priv->id, id))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

const gchar *
ide_toolchain_get_display_name (IdeToolchain  *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return priv->display_name;
}

void
ide_toolchain_set_display_name (IdeToolchain  *self,
                                const gchar   *display_name)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));
  g_return_if_fail (display_name != NULL);

  if (g_set_str (&priv->display_name, display_name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
    }
}

/**
 * ide_toolchain_set_host_triplet:
 * @self: an #IdeToolchain
 * @host_triplet: an #IdeTriplet representing the host architecture of the toolchain
 *
 * Sets the host system of the toolchain
 */
void
ide_toolchain_set_host_triplet (IdeToolchain *self,
                                IdeTriplet   *host_triplet)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_if_fail (IDE_IS_TOOLCHAIN (self));

  if (host_triplet != priv->host_triplet)
    {
      g_clear_pointer (&priv->host_triplet, ide_triplet_unref);
      priv->host_triplet = ide_triplet_ref (host_triplet);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HOST_TRIPLET]);
    }
}

/**
 * ide_toolchain_get_host_triplet:
 * @self: an #IdeToolchain
 *
 * Gets the combination of arch-kernel-system, sometimes referred to as
 * the "host triplet".
 *
 * For Linux based devices, this will generally be something like
 * "x86_64-linux-gnu".
 *
 * Returns: (transfer full): The host system.type of the toolchain
 */
IdeTriplet *
ide_toolchain_get_host_triplet (IdeToolchain *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  return ide_triplet_ref (priv->host_triplet);
}

static const gchar *
ide_toolchain_real_get_tool_for_language (IdeToolchain  *self,
                                          const gchar   *language,
                                          const gchar   *tool_id)
{
  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  g_critical ("%s has not implemented get_tool_for_language()", G_OBJECT_TYPE_NAME (self));

  return NULL;
}

static GHashTable *
ide_toolchain_real_get_tools_for_id (IdeToolchain  *self,
                                     const gchar   *tool_id)
{
  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  g_critical ("%s has not implemented get_tools_for_id()", G_OBJECT_TYPE_NAME (self));

  return NULL;
}

/**
 * ide_toolchain_get_tool_for_language:
 * @self: an #IdeToolchain
 * @language: the language of the tool like %IDE_TOOLCHAIN_LANGUAGE_C.
 * @tool_id: the identifier of the tool like %IDE_TOOLCHAIN_TOOL_CC
 *
 * Gets the path of the specified tool for the requested language.
 * If %IDE_TOOLCHAIN_LANGUAGE_ANY is used in the @language field, the first tool matching @tool_id
 * will be returned.
 *
 * Returns: (transfer none): A string containing the path of the tool for the given language, or
 * %NULL is no tool has been found.
 */
const gchar *
ide_toolchain_get_tool_for_language (IdeToolchain *self,
                                     const gchar  *language,
                                     const gchar  *tool_id)
{
  const gchar *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  ret = IDE_TOOLCHAIN_GET_CLASS (self)->get_tool_for_language (self, language, tool_id);

  IDE_RETURN (ret);
}

/**
 * ide_toolchain_get_tools_for_id:
 * @self: an #IdeToolchain
 * @tool_id: the identifier of the tool like %IDE_TOOLCHAIN_TOOL_CC
 *
 * Gets the list of all the paths to the specified tool id.
 *
 * Returns: (transfer full) (element-type utf8 utf8): A table of language names and paths.
 */
GHashTable *
ide_toolchain_get_tools_for_id (IdeToolchain  *self,
                                const gchar   *tool_id)
{
  GHashTable *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TOOLCHAIN (self), NULL);

  ret = IDE_TOOLCHAIN_GET_CLASS (self)->get_tools_for_id (self, tool_id);

  IDE_RETURN (ret);
}

static void
ide_toolchain_finalize (GObject *object)
{
  IdeToolchain *self = (IdeToolchain *)object;
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->display_name, g_free);
  g_clear_pointer (&priv->host_triplet, ide_triplet_unref);

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
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_toolchain_get_display_name (self));
      break;
    case PROP_HOST_TRIPLET:
      g_value_set_boxed (value, ide_toolchain_get_host_triplet (self));
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
    case PROP_DISPLAY_NAME:
      ide_toolchain_set_display_name (self, g_value_get_string (value));
      break;
    case PROP_HOST_TRIPLET:
      ide_toolchain_set_host_triplet (self, g_value_get_boxed (value));
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

  klass->get_tool_for_language = ide_toolchain_real_get_tool_for_language;
  klass->get_tools_for_id = ide_toolchain_real_get_tools_for_id;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The toolchain identifier",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "The displayable name of the toolchain",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HOST_TRIPLET] =
    g_param_spec_boxed ("host-triplet",
                         "Host Triplet",
                         "The #IdeTriplet object containing the architecture of the machine on which the compiled binary will run",
                         IDE_TYPE_TRIPLET,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_toolchain_init (IdeToolchain *self)
{
  IdeToolchainPrivate *priv = ide_toolchain_get_instance_private (self);
  priv->host_triplet = ide_triplet_new_from_system ();
}
