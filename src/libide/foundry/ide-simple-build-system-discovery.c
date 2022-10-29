/* ide-simple-build-system-discovery.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-simple-build-system-discovery"

#include "config.h"

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <fnmatch.h>

#include "ide-simple-build-system-discovery.h"

typedef struct
{
  gchar *glob;
  gchar *hint;
  guint  is_exact : 1;
  gint   priority;
} IdeSimpleBuildSystemDiscoveryPrivate;

enum {
  PROP_0,
  PROP_GLOB,
  PROP_HINT,
  PROP_PRIORITY,
  N_PROPS
};

static gchar *
ide_simple_build_system_discovery_discover (IdeBuildSystemDiscovery  *discovery,
                                            GFile                    *file,
                                            GCancellable             *cancellable,
                                            gint                     *priority,
                                            GError                  **error);

static void
build_system_discovery_iface_init (IdeBuildSystemDiscoveryInterface *iface)
{
  iface->discover = ide_simple_build_system_discovery_discover;
}

G_DEFINE_TYPE_WITH_CODE (IdeSimpleBuildSystemDiscovery, ide_simple_build_system_discovery, IDE_TYPE_OBJECT,
                         G_ADD_PRIVATE (IdeSimpleBuildSystemDiscovery)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_SYSTEM_DISCOVERY,
                                                build_system_discovery_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ide_simple_build_system_discovery_finalize (GObject *object)
{
  IdeSimpleBuildSystemDiscovery *self = (IdeSimpleBuildSystemDiscovery *)object;
  IdeSimpleBuildSystemDiscoveryPrivate *priv = ide_simple_build_system_discovery_get_instance_private (self);

  g_clear_pointer (&priv->glob, g_free);
  g_clear_pointer (&priv->hint, g_free);

  G_OBJECT_CLASS (ide_simple_build_system_discovery_parent_class)->finalize (object);
}

static void
ide_simple_build_system_discovery_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
  IdeSimpleBuildSystemDiscovery *self = IDE_SIMPLE_BUILD_SYSTEM_DISCOVERY (object);

  switch (prop_id)
    {
    case PROP_GLOB:
      g_value_set_string (value, ide_simple_build_system_discovery_get_glob (self));
      break;

    case PROP_HINT:
      g_value_set_string (value, ide_simple_build_system_discovery_get_hint (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, ide_simple_build_system_discovery_get_priority (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_simple_build_system_discovery_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  IdeSimpleBuildSystemDiscovery *self = IDE_SIMPLE_BUILD_SYSTEM_DISCOVERY (object);

  switch (prop_id)
    {
    case PROP_GLOB:
      ide_simple_build_system_discovery_set_glob (self, g_value_get_string (value));
      break;

    case PROP_HINT:
      ide_simple_build_system_discovery_set_hint (self, g_value_get_string (value));
      break;

    case PROP_PRIORITY:
      ide_simple_build_system_discovery_set_priority (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_simple_build_system_discovery_class_init (IdeSimpleBuildSystemDiscoveryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_simple_build_system_discovery_finalize;
  object_class->get_property = ide_simple_build_system_discovery_get_property;
  object_class->set_property = ide_simple_build_system_discovery_set_property;

  /**
   * IdeSimpleBuildSystemDiscovery:glob:
   *
   * The "glob" property is a glob to match for files within the project
   * directory. This can be used to quickly match the project file, such as
   * "configure.*".
   */
  properties [PROP_GLOB] =
    g_param_spec_string ("glob",
                         "Glob",
                         "The glob to match project filenames",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeSimpleBuildSystemDiscovery:hint:
   *
   * The "hint" property is used from ide_build_system_discovery_discover()
   * if the build file was discovered.
   */
  properties [PROP_HINT] =
    g_param_spec_string ("hint",
                         "Hint",
                         "The hint of the plugin supporting the build system",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeSimpleBuildSystemDiscovery:priority:
   *
   * The "priority" property is the priority of any match.
   */
  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "The priority of the discovery",
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_simple_build_system_discovery_init (IdeSimpleBuildSystemDiscovery *self)
{
}

const gchar *
ide_simple_build_system_discovery_get_glob (IdeSimpleBuildSystemDiscovery *self)
{
  IdeSimpleBuildSystemDiscoveryPrivate *priv = ide_simple_build_system_discovery_get_instance_private (self);
  g_return_val_if_fail (IDE_IS_SIMPLE_BUILD_SYSTEM_DISCOVERY (self), NULL);
  return priv->glob;
}

const gchar *
ide_simple_build_system_discovery_get_hint (IdeSimpleBuildSystemDiscovery *self)
{
  IdeSimpleBuildSystemDiscoveryPrivate *priv = ide_simple_build_system_discovery_get_instance_private (self);
  g_return_val_if_fail (IDE_IS_SIMPLE_BUILD_SYSTEM_DISCOVERY (self), NULL);
  return priv->hint;
}

gint
ide_simple_build_system_discovery_get_priority (IdeSimpleBuildSystemDiscovery *self)
{
  IdeSimpleBuildSystemDiscoveryPrivate *priv = ide_simple_build_system_discovery_get_instance_private (self);
  g_return_val_if_fail (IDE_IS_SIMPLE_BUILD_SYSTEM_DISCOVERY (self), 0);
  return priv->priority;
}

void
ide_simple_build_system_discovery_set_glob (IdeSimpleBuildSystemDiscovery *self,
                                            const gchar                   *glob)
{
  IdeSimpleBuildSystemDiscoveryPrivate *priv = ide_simple_build_system_discovery_get_instance_private (self);

  g_return_if_fail (IDE_IS_SIMPLE_BUILD_SYSTEM_DISCOVERY (self));
  g_return_if_fail (glob != NULL);

  if (g_set_str (&priv->glob, glob))
    {
      priv->is_exact = TRUE;
      for (; priv->is_exact && *glob; glob = g_utf8_next_char (glob))
        {
          gunichar ch = g_utf8_get_char (glob);

          switch (ch)
            {
            case '(': case '!': case '*': case '[': case '{': case '|':
              priv->is_exact = FALSE;
              break;

            default:
              break;
            }
        }
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_GLOB]);
    }
}

void
ide_simple_build_system_discovery_set_hint (IdeSimpleBuildSystemDiscovery *self,
                                            const gchar                   *hint)
{
  IdeSimpleBuildSystemDiscoveryPrivate *priv = ide_simple_build_system_discovery_get_instance_private (self);

  g_return_if_fail (IDE_IS_SIMPLE_BUILD_SYSTEM_DISCOVERY (self));

  if (g_set_str (&priv->hint, hint))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HINT]);
    }
}

void
ide_simple_build_system_discovery_set_priority (IdeSimpleBuildSystemDiscovery *self,
                                                gint                           priority)
{
  IdeSimpleBuildSystemDiscoveryPrivate *priv = ide_simple_build_system_discovery_get_instance_private (self);

  g_return_if_fail (IDE_IS_SIMPLE_BUILD_SYSTEM_DISCOVERY (self));

  if (priority != priv->priority)
    {
      priv->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIORITY]);
    }
}

static gboolean
ide_simple_build_system_discovery_match (IdeSimpleBuildSystemDiscovery *self,
                                         const gchar                   *name)
{
  IdeSimpleBuildSystemDiscoveryPrivate *priv = ide_simple_build_system_discovery_get_instance_private (self);

  g_assert (IDE_IS_SIMPLE_BUILD_SYSTEM_DISCOVERY (self));
  g_assert (name != NULL);

#ifdef FNM_EXTMATCH
  return fnmatch (priv->glob, name, FNM_EXTMATCH) == 0;
#else
  return fnmatch (priv->glob, name, 0) == 0;
#endif
}

static gboolean
ide_simple_build_system_discovery_check_dir (IdeSimpleBuildSystemDiscovery *self,
                                             GFile                         *directory,
                                             GCancellable                  *cancellable)
{
  IdeSimpleBuildSystemDiscoveryPrivate *priv = ide_simple_build_system_discovery_get_instance_private (self);
  g_autoptr(GFileEnumerator) enumerator = NULL;
  gpointer infoptr;

  g_assert (IDE_IS_SIMPLE_BUILD_SYSTEM_DISCOVERY (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (priv->is_exact)
    {
      g_autoptr(GFile) child = g_file_get_child (directory, priv->glob);
      return g_file_query_exists (child, cancellable);
    }

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          NULL);

  while ((infoptr = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      const gchar *name = g_file_info_get_name (info);

      if (ide_simple_build_system_discovery_match (self, name))
        return TRUE;
    }

  return FALSE;
}

static gchar *
ide_simple_build_system_discovery_discover (IdeBuildSystemDiscovery  *discovery,
                                            GFile                    *file,
                                            GCancellable             *cancellable,
                                            gint                     *priority,
                                            GError                  **error)
{
  IdeSimpleBuildSystemDiscovery *self = (IdeSimpleBuildSystemDiscovery *)discovery;
  IdeSimpleBuildSystemDiscoveryPrivate *priv = ide_simple_build_system_discovery_get_instance_private (self);
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) directory = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autofree gchar *name = NULL;

  g_assert (!IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SIMPLE_BUILD_SYSTEM_DISCOVERY (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (priority != NULL);

  *priority = priv->priority;

  if (priv->glob == NULL || priv->hint == NULL)
    goto failure;

  name = g_file_get_basename (file);
  if (ide_simple_build_system_discovery_match (self, name))
    return g_strdup (priv->hint);

  context = ide_object_ref_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, cancellable) != G_FILE_TYPE_DIRECTORY)
    file = directory = g_file_get_parent (file);

  if (ide_simple_build_system_discovery_check_dir (self, file, cancellable) ||
      ide_simple_build_system_discovery_check_dir (self, workdir, cancellable))
    return g_strdup (priv->hint);

failure:
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "No match was discovered");
  return NULL;
}
