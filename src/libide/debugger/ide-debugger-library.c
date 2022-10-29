/* ide-debugger-library.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-library"

#include "config.h"

#include "ide-debugger-library.h"

typedef struct
{
  gchar *id;
  gchar *host_name;
  gchar *target_name;
  GPtrArray *ranges;
} IdeDebuggerLibraryPrivate;

enum {
  PROP_0,
  PROP_ID,
  PROP_HOST_NAME,
  PROP_TARGET_NAME,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeDebuggerLibrary, ide_debugger_library, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_library_finalize (GObject *object)
{
  IdeDebuggerLibrary *self = (IdeDebuggerLibrary *)object;
  IdeDebuggerLibraryPrivate *priv = ide_debugger_library_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->host_name, g_free);
  g_clear_pointer (&priv->ranges, g_ptr_array_unref);
  g_clear_pointer (&priv->target_name, g_free);

  G_OBJECT_CLASS (ide_debugger_library_parent_class)->finalize (object);
}

static void
ide_debugger_library_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeDebuggerLibrary *self = IDE_DEBUGGER_LIBRARY (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_debugger_library_get_id (self));
      break;

    case PROP_HOST_NAME:
      g_value_set_string (value, ide_debugger_library_get_host_name (self));
      break;

    case PROP_TARGET_NAME:
      g_value_set_string (value, ide_debugger_library_get_target_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_library_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeDebuggerLibrary *self = IDE_DEBUGGER_LIBRARY (object);
  IdeDebuggerLibraryPrivate *priv = ide_debugger_library_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_HOST_NAME:
      ide_debugger_library_set_host_name (self, g_value_get_string (value));
      break;

    case PROP_TARGET_NAME:
      ide_debugger_library_set_target_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_library_class_init (IdeDebuggerLibraryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_library_finalize;
  object_class->get_property = ide_debugger_library_get_property;
  object_class->set_property = ide_debugger_library_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The identifier for library",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_HOST_NAME] =
    g_param_spec_string ("host-name",
                         "Host Name",
                         "The host name for the library",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TARGET_NAME] =
    g_param_spec_string ("target-name",
                         "Target Name",
                         "The target name for the library",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_debugger_library_init (IdeDebuggerLibrary *self)
{
  IdeDebuggerLibraryPrivate *priv = ide_debugger_library_get_instance_private (self);

  priv->ranges = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_debugger_address_range_free);
}

IdeDebuggerLibrary *
ide_debugger_library_new (const gchar *id)
{
  return g_object_new (IDE_TYPE_DEBUGGER_LIBRARY,
                       "id", id,
                       NULL);
}

const gchar *
ide_debugger_library_get_id (IdeDebuggerLibrary *self)
{
  IdeDebuggerLibraryPrivate *priv = ide_debugger_library_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_LIBRARY (self), NULL);

  return priv->id;
}

const gchar *
ide_debugger_library_get_host_name (IdeDebuggerLibrary *self)
{
  IdeDebuggerLibraryPrivate *priv = ide_debugger_library_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_LIBRARY (self), NULL);

  return priv->host_name;
}

void
ide_debugger_library_set_host_name (IdeDebuggerLibrary *self,
                                    const gchar        *host_name)
{
  IdeDebuggerLibraryPrivate *priv = ide_debugger_library_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_LIBRARY (self));

  if (g_set_str (&priv->host_name, host_name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HOST_NAME]);
    }
}

const gchar *
ide_debugger_library_get_target_name (IdeDebuggerLibrary *self)
{
  IdeDebuggerLibraryPrivate *priv = ide_debugger_library_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_LIBRARY (self), NULL);

  return priv->target_name;
}

void
ide_debugger_library_set_target_name (IdeDebuggerLibrary *self,
                                      const gchar        *target_name)
{
  IdeDebuggerLibraryPrivate *priv = ide_debugger_library_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_LIBRARY (self));

  if (g_set_str (&priv->target_name, target_name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TARGET_NAME]);
    }
}

/**
 * ide_debugger_library_get_ranges:
 * @self: An #IdeDebuggerLibrary
 *
 * Gets the list of address ranges for the library.
 *
 * Returns: (transfer none) (element-type Ide.DebuggerAddressRange): a #GPtrArray
 *   containing the list of address ranges.
 */
GPtrArray *
ide_debugger_library_get_ranges (IdeDebuggerLibrary *self)
{
  IdeDebuggerLibraryPrivate *priv = ide_debugger_library_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_LIBRARY (self), NULL);

  return priv->ranges;
}

/**
 * ide_debugger_library_add_range:
 * @self: An #IdeDebuggerLibrary
 * @range: the address range of the library
 *
 * Adds @range to the list of ranges for which the library is mapped in
 * the inferior's address space.
 */
void
ide_debugger_library_add_range (IdeDebuggerLibrary            *self,
                                const IdeDebuggerAddressRange *range)
{
  IdeDebuggerLibraryPrivate *priv = ide_debugger_library_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_LIBRARY (self));
  g_return_if_fail (range != NULL);

  /* NOTE: It is unclear to me if a single library can have different
   *       ELF sections from one library mapped into different, non-contiguous
   *       regions within the inferior's address space.
   */

  g_ptr_array_add (priv->ranges, ide_debugger_address_range_copy (range));
}

gint
ide_debugger_library_compare (IdeDebuggerLibrary *a,
                              IdeDebuggerLibrary *b)
{
  IdeDebuggerLibraryPrivate *priv_a = ide_debugger_library_get_instance_private (a);
  IdeDebuggerLibraryPrivate *priv_b = ide_debugger_library_get_instance_private (b);

  return g_strcmp0 (priv_a->id, priv_b->id);
}
