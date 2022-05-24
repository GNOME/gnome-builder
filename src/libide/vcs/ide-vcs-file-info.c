/* ide-vcs-file-info.c
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

#define G_LOG_DOMAIN "ide-vcs-file-info"

#include "config.h"

#include "ide-vcs-enums.h"
#include "ide-vcs-file-info.h"

typedef struct
{
  GFile *file;
  IdeVcsFileStatus status;
} IdeVcsFileInfoPrivate;

enum {
  PROP_0,
  PROP_FILE,
  PROP_STATUS,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeVcsFileInfo, ide_vcs_file_info, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

/**
 * ide_vcs_file_info_get_file:
 * @self: a #IdeVcsFileInfo
 *
 * Gets the file the #IdeVcsFileInfo describes.
 *
 * Returns: (transfer none): a #GFile
 */
GFile *
ide_vcs_file_info_get_file (IdeVcsFileInfo *self)
{
  IdeVcsFileInfoPrivate *priv = ide_vcs_file_info_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_VCS_FILE_INFO (self), NULL);

  return priv->file;
}

static void
ide_vcs_file_info_set_file (IdeVcsFileInfo *self,
                            GFile          *file)
{
  IdeVcsFileInfoPrivate *priv = ide_vcs_file_info_get_instance_private (self);

  g_return_if_fail (IDE_IS_VCS_FILE_INFO (self));
  g_return_if_fail (G_IS_FILE (file));

  g_set_object (&priv->file, file);
}

IdeVcsFileStatus
ide_vcs_file_info_get_status (IdeVcsFileInfo *self)
{
  IdeVcsFileInfoPrivate *priv = ide_vcs_file_info_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_VCS_FILE_INFO (self), 0);

  return priv->status;
}

void
ide_vcs_file_info_set_status (IdeVcsFileInfo   *self,
                              IdeVcsFileStatus  status)
{
  IdeVcsFileInfoPrivate *priv = ide_vcs_file_info_get_instance_private (self);

  g_return_if_fail (IDE_IS_VCS_FILE_INFO (self));

  if (priv->status != status)
    {
      priv->status = status;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STATUS]);
    }
}

static void
ide_vcs_file_info_finalize (GObject *object)
{
  IdeVcsFileInfo *self = (IdeVcsFileInfo *)object;
  IdeVcsFileInfoPrivate *priv = ide_vcs_file_info_get_instance_private (self);

  g_clear_object (&priv->file);

  G_OBJECT_CLASS (ide_vcs_file_info_parent_class)->finalize (object);
}

static void
ide_vcs_file_info_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeVcsFileInfo *self = IDE_VCS_FILE_INFO (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_vcs_file_info_get_file (self));
      break;

    case PROP_STATUS:
      g_value_set_enum (value, ide_vcs_file_info_get_status (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_vcs_file_info_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeVcsFileInfo *self = IDE_VCS_FILE_INFO (object);

  switch (prop_id)
    {
    case PROP_FILE:
      ide_vcs_file_info_set_file (self, g_value_get_object (value));
      break;

    case PROP_STATUS:
      ide_vcs_file_info_set_status (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_vcs_file_info_class_init (IdeVcsFileInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_vcs_file_info_finalize;
  object_class->get_property = ide_vcs_file_info_get_property;
  object_class->set_property = ide_vcs_file_info_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The file within the working directory",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_STATUS] =
    g_param_spec_enum ("status",
                       "Status",
                       "The file status within the VCS",
                       IDE_TYPE_VCS_FILE_STATUS,
                       IDE_VCS_FILE_STATUS_UNCHANGED,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_vcs_file_info_init (IdeVcsFileInfo *self)
{
}

IdeVcsFileInfo *
ide_vcs_file_info_new (GFile *file)
{
  return g_object_new (IDE_TYPE_VCS_FILE_INFO,
                       "file", file,
                       NULL);
}
