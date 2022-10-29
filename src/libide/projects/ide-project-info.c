/* ide-project-info.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#define G_LOG_DOMAIN "ide-project-info"

#include "config.h"

#include <glib/gi18n.h>
#include <string.h>

#include "ide-gfile-private.h"

#include "ide-project-info.h"
#include "ide-project-info-private.h"

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

  gchar      *id;
  IdeDoap    *doap;
  GDateTime  *last_modified_at;
  GFile      *directory;
  GFile      *directory_translated;
  GFile      *file;
  GFile      *file_translated;
  gchar      *build_system_name;
  gchar      *build_system_hint;
  gchar      *name;
  gchar      *description;
  gchar     **languages;
  gchar      *vcs_uri;
  GIcon      *icon;

  gint        priority;

  guint       is_recent : 1;
};

G_DEFINE_FINAL_TYPE (IdeProjectInfo, ide_project_info, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUILD_SYSTEM_HINT,
  PROP_BUILD_SYSTEM_NAME,
  PROP_DESCRIPTION,
  PROP_DIRECTORY,
  PROP_DOAP,
  PROP_FILE,
  PROP_ICON,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_IS_RECENT,
  PROP_LANGUAGES,
  PROP_LAST_MODIFIED_AT,
  PROP_NAME,
  PROP_PRIORITY,
  PROP_VCS_URI,
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

  if (languages != self->languages)
    {
      g_strfreev (self->languages);
      self->languages = g_strdupv (languages);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGES]);
    }
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

  return self->directory_translated;
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

  return self->file_translated;
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
ide_project_info_get_build_system_hint (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->build_system_hint;
}

void
ide_project_info_set_build_system_hint (IdeProjectInfo *self,
                                        const gchar    *build_system_hint)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  if (g_set_str (&self->build_system_hint, build_system_hint))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUILD_SYSTEM_HINT]);
    }
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

  if (g_set_str (&self->build_system_name, build_system_name))
    {
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

  if (g_set_str (&self->description, description))
    {
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

  if (g_set_str (&self->name, name))
    {
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
    {
      g_clear_object (&self->directory_translated);
      self->directory_translated = _ide_g_file_readlink (directory);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTORY]);
    }
}

void
ide_project_info_set_file (IdeProjectInfo *self,
                           GFile          *file)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));
  g_return_if_fail (!file || G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    {
      g_clear_object (&self->file_translated);
      self->file_translated = _ide_g_file_readlink (file);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
    }
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

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->last_modified_at, g_date_time_unref);
  g_clear_pointer (&self->build_system_hint, g_free);
  g_clear_pointer (&self->build_system_name, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->languages, g_strfreev);
  g_clear_pointer (&self->vcs_uri, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_object (&self->directory);
  g_clear_object (&self->directory_translated);
  g_clear_object (&self->file);
  g_clear_object (&self->file_translated);
  g_clear_object (&self->icon);

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
    case PROP_BUILD_SYSTEM_HINT:
      g_value_set_string (value, ide_project_info_get_build_system_hint (self));
      break;

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

    case PROP_ICON:
      g_value_set_object (value, ide_project_info_get_icon (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_project_info_get_id (self));
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

    case PROP_VCS_URI:
      g_value_set_string (value, ide_project_info_get_vcs_uri (self));
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
    case PROP_BUILD_SYSTEM_HINT:
      ide_project_info_set_build_system_hint (self, g_value_get_string (value));
      break;

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

    case PROP_ICON:
      ide_project_info_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      ide_project_info_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_ID:
      ide_project_info_set_id (self, g_value_get_string (value));
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

    case PROP_VCS_URI:
      ide_project_info_set_vcs_uri (self, g_value_get_string (value));
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

  properties [PROP_BUILD_SYSTEM_HINT] =
    g_param_spec_string ("build-system-hint",
                         "Build System hint",
                         "Build System hint",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_BUILD_SYSTEM_NAME] =
    g_param_spec_string ("build-system-name",
                         "Build System name",
                         "Build System name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DESCRIPTION] =
    g_param_spec_string ("description",
                         "Description",
                         "The project description.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON] =
    g_param_spec_object ("icon",
                         "Icon",
                         "The icon for the project",
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "The icon-name for the project",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The identifier for the project",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The project name.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         "Directory",
                         "The project directory.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DOAP] =
    g_param_spec_object ("doap",
                         "DOAP",
                         "A DOAP describing the project.",
                         IDE_TYPE_DOAP,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The toplevel project file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_IS_RECENT] =
    g_param_spec_boolean ("is-recent",
                          "Is Recent",
                          "Is Recent",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LANGUAGES] =
    g_param_spec_boxed ("languages",
                        "Languages",
                        "Languages",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LAST_MODIFIED_AT] =
    g_param_spec_boxed ("last-modified-at",
                        "Last Modified At",
                        "Last Modified At",
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "The priority of the project information type.",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_VCS_URI] =
    g_param_spec_string ("vcs-uri",
                         "Vcs Uri",
                         "The VCS URI of the project, in case it is not local",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

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

  if (info1 == info2)
    return 0;

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

/**
 * ide_project_info_get_vcs_uri:
 * @self: an #IdeProjectInfo
 *
 * Gets the VCS URI for the project info. This should be set with the
 * remote URI for the version control system. It can be used to clone the
 * project when activated from the greeter.
 *
 * Returns: (transfer none) (nullable): a #IdeVcsUri or %NULL
 */
const gchar *
ide_project_info_get_vcs_uri (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->vcs_uri;
}

void
ide_project_info_set_vcs_uri (IdeProjectInfo *self,
                              const gchar    *vcs_uri)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  if (g_set_str (&self->vcs_uri, vcs_uri))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VCS_URI]);
    }
}

IdeProjectInfo *
ide_project_info_new (void)
{
  return g_object_new (IDE_TYPE_PROJECT_INFO, NULL);
}

/**
 * ide_project_info_equal:
 * @self: a #IdeProjectInfo
 * @other: a #IdeProjectInfo
 *
 * This function will check to see if information about @self and @other are
 * similar enough that a request to open @other would instead activate
 * @self. This is useful when a user tries to open the same project twice.
 *
 * However, some case is taken to ensure that things like the build system
 * are the same so that a project may be opened twice with two build systems
 * as is sometimes necessary when projects are porting to a new build
 * system.
 *
 * Returns: %TRUE if @self and @other are the same project and similar
 *   enough to be considered equal.
 */
gboolean
ide_project_info_equal (IdeProjectInfo *self,
                        IdeProjectInfo *other)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), FALSE);
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (other), FALSE);

  if (!self->file || !other->file ||
      !g_file_equal (self->file, other->file))
    {
      if (!self->directory || !other->directory ||
          !g_file_equal (self->directory, other->directory))
        return FALSE;
    }

  /* build-system only set in one of the project-info?
   * That's fine, we'll consider them the same to avoid over
   * activating a second workbench
   */
  if ((!self->build_system_name && other->build_system_name) ||
      (self->build_system_name && !other->build_system_name))
    return TRUE;

  return ide_str_equal0 (self->build_system_name, other->build_system_name);
}

const gchar *
ide_project_info_get_id (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  if (!self->id && self->directory)
    self->id = g_file_get_basename (self->directory);

  if (!self->id && self->file)
    {
      if (g_file_query_file_type (self->file, 0, NULL) == G_FILE_TYPE_DIRECTORY)
        self->id = g_file_get_basename (self->file);
      else
        {
          g_autoptr(GFile) parent = g_file_get_parent (self->file);
          self->id = g_file_get_basename (parent);
        }
    }

  if (!self->id && self->doap)
    self->id = g_strdup (ide_doap_get_name (self->doap));

  if (!self->id && self->vcs_uri)
    {
      const gchar *path = self->vcs_uri;

      if (strstr (path, "//"))
        path = strstr (path, "//") + 1;

      if (strchr (path, '/'))
        path = strchr (path, '/');
      else if (strrchr (path, ':'))
        path = strrchr (path, ':');

      self->id = g_path_get_basename (path);
    }

  return self->id;
}

void
ide_project_info_set_id (IdeProjectInfo *self,
                         const gchar    *id)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  if (g_set_str (&self->id, id))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

/**
 * ide_project_info_get_icon:
 * @self: a #IdeProjectInfo
 *
 * Gets the #IdeProjectInfo:icon property.
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 */
GIcon *
ide_project_info_get_icon (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->icon;
}

void
ide_project_info_set_icon (IdeProjectInfo *self,
                           GIcon          *icon)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));
  g_return_if_fail (!icon || G_IS_ICON (icon));

  if (g_set_object (&self->icon, icon))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON]);
}

void
ide_project_info_set_icon_name (IdeProjectInfo *self,
                                const gchar    *icon_name)
{
  g_autoptr(GIcon) icon = NULL;

  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  if (icon_name != NULL)
    icon = g_themed_icon_new (icon_name);
  ide_project_info_set_icon (self, icon);
}

GFile *
_ide_project_info_get_real_file (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);
  return self->file;
}

GFile *
_ide_project_info_get_real_directory (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);
  return self->directory;
}
