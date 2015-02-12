/* ide-project-file.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-project-file.h"

typedef struct
{
  GFile     *file;
  GFileInfo *file_info;
} IdeProjectFilePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeProjectFile, ide_project_file,
                            IDE_TYPE_PROJECT_ITEM)

enum {
  PROP_0,
  PROP_FILE,
  PROP_FILE_INFO,
  PROP_NAME,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

const gchar *
ide_project_file_get_name (IdeProjectFile *self)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), NULL);

  return g_file_info_get_name (priv->file_info);
}

GFile *
ide_project_file_get_file (IdeProjectFile *file)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (file);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (file), NULL);

  return priv->file;
}

void
ide_project_file_set_file (IdeProjectFile *self,
                           GFile          *file)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_if_fail (IDE_IS_PROJECT_FILE (self));
  g_return_if_fail (!file || G_IS_FILE (file));

  if (g_set_object (&priv->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);
}

GFileInfo *
ide_project_file_get_file_info (IdeProjectFile *file)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (file);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (file), NULL);

  return priv->file_info;
}

void
ide_project_file_set_file_info (IdeProjectFile *file,
                                GFileInfo      *file_info)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (file);

  g_return_if_fail (IDE_IS_PROJECT_FILE (file));
  g_return_if_fail (!file_info || G_IS_FILE_INFO (file_info));

  if (g_set_object (&priv->file_info, file_info))
    g_object_notify_by_pspec (G_OBJECT (file), gParamSpecs [PROP_FILE_INFO]);
}

static void
ide_project_file_finalize (GObject *object)
{
  IdeProjectFile *self = (IdeProjectFile *)object;
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_clear_object (&priv->file_info);

  G_OBJECT_CLASS (ide_project_file_parent_class)->finalize (object);
}

static void
ide_project_file_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeProjectFile *self = IDE_PROJECT_FILE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_project_file_get_file (self));
      break;

    case PROP_FILE_INFO:
      g_value_set_object (value, ide_project_file_get_file_info (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_project_file_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_file_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeProjectFile *self = IDE_PROJECT_FILE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      ide_project_file_set_file (self, g_value_get_object (value));
      break;

    case PROP_FILE_INFO:
      ide_project_file_set_file_info (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_file_class_init (IdeProjectFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_project_file_finalize;
  object_class->get_property = ide_project_file_get_property;
  object_class->set_property = ide_project_file_set_property;

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("A GFile to the underlying file."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE,
                                   gParamSpecs [PROP_FILE]);

  gParamSpecs [PROP_FILE_INFO] =
    g_param_spec_object ("file-info",
                         _("File Info"),
                         _("The file info for the project file."),
                         G_TYPE_FILE_INFO,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE_INFO,
                                   gParamSpecs [PROP_FILE_INFO]);

  gParamSpecs [PROP_NAME] =
    g_param_spec_string ("name",
                         _("Name"),
                         _("The shortname of the file."),
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NAME,
                                   gParamSpecs [PROP_NAME]);
}

static void
ide_project_file_init (IdeProjectFile *self)
{
}
