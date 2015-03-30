/* ide-project-info.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-project-info"

#include <glib/gi18n.h>

#include "ide-project-info.h"

/**
 * SECTION:ide-project-info:
 * @title: IdeProjectInfo
 * @short_description: Information about a project not yet loaded
 *
 * This class contains information about a project that can be loaded.
 * This information should be used to display a list of available projects.
 */

struct _IdeProjectInfo
{
  GObject  parent_instance;

  GFile   *directory;
  GFile   *file;
  gchar   *name;
};

G_DEFINE_TYPE (IdeProjectInfo, ide_project_info, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DIRECTORY,
  PROP_FILE,
  PROP_NAME,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * ide_project_info_get_directory:
 * @self: (in): A #IdeProjectInfo.
 *
 * Gets the #IdeProjectInfo:directory property.
 * This is the directory containing the project (if known).
 *
 * Returns: (nullable) (transfer none): A #GFile.
 */
GFile *
ide_project_info_get_directory (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->directory;
}

/**
 * ide_project_info_get_file:
 * @self: (in): A #IdeProjectInfo.
 *
 * Gets the #IdeProjectInfo:file property.
 * This is the project file (such as configure.ac) of the project.
 *
 * Returns: (nullable) (transfer none): A #GFile.
 */
GFile *
ide_project_info_get_file (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->file;
}

const gchar *
ide_project_info_get_name (IdeProjectInfo *self)
{
  g_return_val_if_fail (IDE_IS_PROJECT_INFO (self), NULL);

  return self->name;
}

void
ide_project_info_set_directory (IdeProjectInfo *self,
                                GFile          *directory)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));

  if (g_set_object (&self->directory, directory))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_DIRECTORY]);
}

void
ide_project_info_set_file (IdeProjectInfo *self,
                           GFile          *file)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));
  g_return_if_fail (!file || G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);
}

void
ide_project_info_set_name (IdeProjectInfo *self,
                           const gchar    *name)
{
  g_return_if_fail (IDE_IS_PROJECT_INFO (self));

  if (name != self->name)
    {
      g_free (self->name);
      self->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_NAME]);
    }
}

static void
ide_project_info_finalize (GObject *object)
{
  IdeProjectInfo *self = (IdeProjectInfo *)object;

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
    case PROP_DIRECTORY:
      g_value_set_object (value, ide_project_info_get_directory (self));
      break;

    case PROP_FILE:
      g_value_set_object (value, ide_project_info_get_file (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_project_info_get_name (self));
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
    case PROP_DIRECTORY:
      ide_project_info_set_directory (self, g_value_get_object (value));
      break;

    case PROP_FILE:
      ide_project_info_set_file (self, g_value_get_object (value));
      break;

    case PROP_NAME:
      ide_project_info_set_name (self, g_value_get_string (value));
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

  gParamSpecs [PROP_NAME] =
    g_param_spec_string ("name",
                         _("Name"),
                         _("The project name"),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NAME, gParamSpecs [PROP_NAME]);

  gParamSpecs [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         _("Directory"),
                         _("The project directory."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DIRECTORY, gParamSpecs [PROP_DIRECTORY]);

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The toplevel project file"),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE, gParamSpecs [PROP_FILE]);
}

static void
ide_project_info_init (IdeProjectInfo *self)
{
}
