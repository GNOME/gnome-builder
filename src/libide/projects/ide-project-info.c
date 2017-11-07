/* ide-project-info.c
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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#define G_LOG_DOMAIN "ide-project-info"

#include <glib/gi18n.h>
#include <string.h>

#include "ide-macros.h"
#include "projects/ide-project-info.h"

/**
 * SECTION:ideprojectinfo:
 * @title: IdeProjectInfo
 * @short_description: Information about a project not yet loaded
 *
 * This class contains information about a project that can be loaded.
 * This information should be used to display a list of available projects.
 */

struct _IdeProjectInfo
{
  GObject     parent_instance;

  IdeDoap    *doap;
  GDateTime  *last_modified_at;
  GFile      *directory;
  GFile      *file;
  gchar      *build_system_name;
  gchar      *name;
  gchar      *description;
  gchar     **languages;

  gint        priority;

  guint       is_recent : 1;
};

G_DEFINE_TYPE (IdeProjectInfo, ide_project_info, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUILD_SYSTEM_NAME,
  PROP_DESCRIPTION,
  PROP_DIRECTORY,
  PROP_DOAP,
  PROP_FILE,
  PROP_IS_RECENT,
  PROP_LANGUAGES,
  PROP_LAST_MODIFIED_AT,
  PROP_NAME,
  PROP_PRIORITY,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

/**
 * ide_project_info_get_doap:
 *
 *
 * Returns: (nullable) (transfer none): An #IdeDoap or %NULL.
 */
IdeDoap *
ide_project_info_get_doap (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->doap;
}

void
ide_project_info_set_doap (IdeProjectInfo *self,
                           IdeDoap        *doap)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));
  g_return_if_fail (!doap || IDE_IS_DOAP (doap));

  if (g_set_object (&self->doap, doap))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DOAP]);
}

/**
 * ide_project_info_get_languages:
 *
 * Returns: (transfer none): An array of language names.
 */
const gchar * const *
ide_project_info_get_languages (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return (const gchar * const *)self->languages;
}

void
ide_project_info_set_languages (IdeProjectInfo  *self,
                                gchar          **languages)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  g_strfreev (self->languages);
  self->languages = g_strdupv (languages);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGES]);
}

gint
ide_project_info_get_priority (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), 0);

  return self->priority;
}

void
ide_project_info_set_priority (IdeProjectInfo *self,
                               gint            priority)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  if (self->priority != priority)
    {
      self->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIORITY]);
    }
}

/**
 * ide_project_info_get_directory:
 * @self: (in): an #IdeProjectInfo.
 *
 * Gets the #IdeProjectInfo:directory property.
 * This is the directory containing the project (if known).
 *
 * Returns: (nullable) (transfer none): a #GFile.
 */
GFile *
ide_project_info_get_directory (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->directory;
}

/**
 * ide_project_info_get_file:
 * @self: (in): an #IdeProjectInfo.
 *
 * Gets the #IdeProjectInfo:file property.
 * This is the project file (such as configure.ac) of the project.
 *
 * Returns: (nullable) (transfer none): a #GFile.
 */
GFile *
ide_project_info_get_file (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->file;
}

/**
 * ide_project_info_get_last_modified_at:
 *
 *
 * Returns: (transfer none) (nullable): a #GDateTime or %NULL.
 */
GDateTime *
ide_project_info_get_last_modified_at (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->last_modified_at;
}

const gchar *
ide_project_info_get_build_system_name (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->build_system_name;
}

void
ide_project_info_set_build_system_name (IdeProjectInfo *self,
                                        const gchar    *build_system_name)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  if (!ide_str_equal0 (self->build_system_name, build_system_name))
    {
      g_free (self->build_system_name);
      self->build_system_name = g_strdup (build_system_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUILD_SYSTEM_NAME]);
    }
}

const gchar *
ide_project_info_get_description (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->description;
}

void
ide_project_info_set_description (IdeProjectInfo *self,
                                  const gchar    *description)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  if (!ide_str_equal0 (self->description, description))
    {
      g_free (self->description);
      self->description = g_strdup (description);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DESCRIPTION]);
    }
}

const gchar *
ide_project_info_get_name (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->name;
}

void
ide_project_info_set_name (IdeProjectInfo *self,
                           const gchar    *name)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  if (!ide_str_equal0 (self->name, name))
    {
      g_free (self->name);
      self->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

void
ide_project_info_set_directory (IdeProjectInfo *self,
                                GFile          *directory)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));

  if (g_set_object (&self->directory, directory))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTORY]);
}

void
ide_project_info_set_file (IdeProjectInfo *self,
                           GFile          *file)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));
  g_return_if_fail (!file || G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
}

void
ide_project_info_set_last_modified_at (IdeProjectInfo *self,
                                       GDateTime      *last_modified_at)
{
  g_assert (IDE_IS_PROJECT_INFO (self));

  if (last_modified_at != self->last_modified_at)
    {
      g_clear_pointer (&self->last_modified_at, g_date_time_unref);
      self->last_modified_at = last_modified_at ? g_date_time_ref (last_modified_at) : NULL;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_MODIFIED_AT]);
    }
}

gboolean
ide_project_info_get_is_recent (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), FALSE);

  return self->is_recent;
}

void
ide_project_info_set_is_recent (IdeProjectInfo *self,
                                gboolean        is_recent)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  is_recent = !!is_recent;

  if (self->is_recent != is_recent)
    {
      self->is_recent = is_recent;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_RECENT]);
    }
}

static void
ide_project_info_finalize (GObject *object)
{
  IdeProjectInfo *self = (IdeProjectInfo *)object;

  g_clear_pointer (&self->last_modified_at, g_date_time_unref);
  g_clear_pointer (&self->build_system_name, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->languages, g_strfreev);
  g_clear_pointer (&self->name, g_free);
  g_clear_object (&self->directory);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (ide_project_info_parent_class)->finalize (object);
}

static void
ide_project_info_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeProjectInfo *self = IDE_PROJECT_INFO (object);

  switch (prop_id)
    {
    case PROP_BUILD_SYSTEM_NAME:
      g_value_set_string (value, ide_project_info_get_build_system_name (self));
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, ide_project_info_get_description (self));
      break;

    case PROP_DIRECTORY:
      g_value_set_object (value, ide_project_info_get_directory (self));
      break;

    case PROP_DOAP:
      g_value_set_object (value, ide_project_info_get_doap (self));
      break;

    case PROP_FILE:
      g_value_set_object (value, ide_project_info_get_file (self));
      break;

    case PROP_IS_RECENT:
      g_value_set_boolean (value, ide_project_info_get_is_recent (self));
      break;

    case PROP_LANGUAGES:
      g_value_set_boxed (value, ide_project_info_get_languages (self));
      break;

    case PROP_LAST_MODIFIED_AT:
      g_value_set_boxed (value, ide_project_info_get_last_modified_at (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_project_info_get_name (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, ide_project_info_get_priority (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_info_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeProjectInfo *self = IDE_PROJECT_INFO (object);

  switch (prop_id)
    {
    case PROP_BUILD_SYSTEM_NAME:
      ide_project_info_set_build_system_name (self, g_value_get_string (value));
      break;

    case PROP_DESCRIPTION:
      ide_project_info_set_description (self, g_value_get_string (value));
      break;

    case PROP_DIRECTORY:
      ide_project_info_set_directory (self, g_value_get_object (value));
      break;

    case PROP_DOAP:
      ide_project_info_set_doap (self, g_value_get_object (value));
      break;

    case PROP_FILE:
      ide_project_info_set_file (self, g_value_get_object (value));
      break;

    case PROP_IS_RECENT:
      ide_project_info_set_is_recent (self, g_value_get_boolean (value));
      break;

    case PROP_LANGUAGES:
      ide_project_info_set_languages (self, g_value_get_boxed (value));
      break;

    case PROP_LAST_MODIFIED_AT:
      ide_project_info_set_last_modified_at (self, g_value_get_boxed (value));
      break;

    case PROP_NAME:
      ide_project_info_set_name (self, g_value_get_string (value));
      break;

    case PROP_PRIORITY:
      ide_project_info_set_priority (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_info_class_init (IdeProjectInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_project_info_finalize;
  object_class->get_property = ide_project_info_get_property;
  object_class->set_property = ide_project_info_set_property;

  properties [PROP_BUILD_SYSTEM_NAME] =
    g_param_spec_string ("build-system-name",
                         "Build System name",
                         "Build System name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DESCRIPTION] =
    g_param_spec_string ("description",
                         "Description",
                         "The project description.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The project name.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         "Directory",
                         "The project directory.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DOAP] =
    g_param_spec_object ("doap",
                         "DOAP",
                         "A DOAP describing the project.",
                         IDE_TYPE_DOAP,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The toplevel project file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_IS_RECENT] =
    g_param_spec_boolean ("is-recent",
                          "Is Recent",
                          "Is Recent",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LANGUAGES] =
    g_param_spec_boxed ("languages",
                        "Languages",
                        "Languages",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LAST_MODIFIED_AT] =
    g_param_spec_boxed ("last-modified-at",
                        "Last Modified At",
                        "Last Modified At",
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "The priority of the project information type.",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_project_info_init (IdeProjectInfo *self)
{
}

gint
ide_project_info_compare (IdeProjectInfo *info1,
                          IdeProjectInfo *info2)
{
  const gchar *name1;
  const gchar *name2;
  GDateTime *dt1;
  GDateTime *dt2;
  gint ret;
  gint prio1;
  gint prio2;

  g_assert (IDE_IS_PROJECT_INFO (info1));
  g_assert (IDE_IS_PROJECT_INFO (info2));

  prio1 = ide_project_info_get_priority (info1);
  prio2 = ide_project_info_get_priority (info2);

  if (prio1 != prio2)
    return prio1 - prio2;

  dt1 = ide_project_info_get_last_modified_at (info1);
  dt2 = ide_project_info_get_last_modified_at (info2);

  ret = g_date_time_compare (dt2, dt1);

  if (ret != 0)
    return ret;

  name1 = ide_project_info_get_name (info1);
  name2 = ide_project_info_get_name (info2);

  if (name1 == NULL)
    return 1;
  else if (name2 == NULL)
    return -1;
  else
    return strcasecmp (name1, name2);
}
