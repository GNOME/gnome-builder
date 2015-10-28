/* gb-project-file.c
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

#include <glib/gi18n.h>

#include "gb-project-file.h"

struct _GbProjectFile
{
  GObject    parent_instance;

  GFile     *file;
  GFileInfo *file_info;
};

G_DEFINE_TYPE (GbProjectFile, gb_project_file, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_FILE,
  PROP_FILE_INFO,
  PROP_ICON_NAME,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

gint
gb_project_file_compare (GbProjectFile *a,
                         GbProjectFile *b)
{
  const gchar *display_name_a = g_file_info_get_display_name (a->file_info);
  const gchar *display_name_b = g_file_info_get_display_name (b->file_info);
  gchar *casefold_a = NULL;
  gchar *casefold_b = NULL;
  gboolean ret;

  casefold_a = g_utf8_collate_key_for_filename (display_name_a, -1);
  casefold_b = g_utf8_collate_key_for_filename (display_name_b, -1);

  ret = strcmp (casefold_a, casefold_b);

  g_free (casefold_a);
  g_free (casefold_b);

  return ret;
}

gint
gb_project_file_compare_directories_first (GbProjectFile *a,
                                           GbProjectFile *b)
{
  GFileType file_type_a = g_file_info_get_file_type (a->file_info);
  GFileType file_type_b = g_file_info_get_file_type (b->file_info);
  gint dir_a = (file_type_a == G_FILE_TYPE_DIRECTORY);
  gint dir_b = (file_type_b == G_FILE_TYPE_DIRECTORY);
  gint ret;

  ret = dir_b - dir_a;
  if (ret == 0)
    ret = gb_project_file_compare (a, b);

  return ret;
}


static void
gb_project_file_finalize (GObject *object)
{
  GbProjectFile *self = (GbProjectFile *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->file_info);

  G_OBJECT_CLASS (gb_project_file_parent_class)->finalize (object);
}

static void
gb_project_file_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GbProjectFile *self = GB_PROJECT_FILE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, gb_project_file_get_display_name (self));
      break;

    case PROP_ICON_NAME:
      g_value_set_static_string (value, gb_project_file_get_icon_name (self));
      break;

    case PROP_FILE:
      g_value_set_object (value, gb_project_file_get_file (self));
      break;

    case PROP_FILE_INFO:
      g_value_set_object (value, gb_project_file_get_file_info (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_project_file_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbProjectFile *self = GB_PROJECT_FILE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gb_project_file_set_file (self, g_value_get_object (value));
      break;

    case PROP_FILE_INFO:
      gb_project_file_set_file_info (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_project_file_class_init (GbProjectFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_project_file_finalize;
  object_class->get_property = gb_project_file_get_property;
  object_class->set_property = gb_project_file_set_property;

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "Display Name",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "Icon Name",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "File",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE_INFO] =
    g_param_spec_object ("file-info",
                         "File Info",
                         "File Info",
                         G_TYPE_FILE_INFO,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gb_project_file_init (GbProjectFile *self)
{
}

GbProjectFile *
gb_project_file_new (GFile     *file,
                     GFileInfo *file_info)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_FILE_INFO (file_info), NULL);

  return g_object_new (GB_TYPE_PROJECT_FILE,
                       "file", file,
                       "file-info", file_info,
                       NULL);
}

GFile *
gb_project_file_get_file (GbProjectFile *self)
{
  g_return_val_if_fail (GB_IS_PROJECT_FILE (self), NULL);

  return self->file;
}

void
gb_project_file_set_file (GbProjectFile *self,
                          GFile         *file)
{
  g_return_if_fail (GB_IS_PROJECT_FILE (self));
  g_return_if_fail (!file || G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
}

GFileInfo *
gb_project_file_get_file_info (GbProjectFile *self)
{
  g_return_val_if_fail (GB_IS_PROJECT_FILE (self), NULL);

  return self->file_info;
}

void
gb_project_file_set_file_info (GbProjectFile *self,
                               GFileInfo     *file_info)
{
  g_return_if_fail (GB_IS_PROJECT_FILE (self));
  g_return_if_fail (!file_info || G_IS_FILE_INFO (file_info));

  if (g_set_object (&self->file_info, file_info))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE_INFO]);
}

gboolean
gb_project_file_get_is_directory (GbProjectFile *self)
{
  g_return_val_if_fail (GB_IS_PROJECT_FILE (self), FALSE);

  if (self->file_info != NULL)
    return g_file_info_get_file_type (self->file_info) == G_FILE_TYPE_DIRECTORY;

  return G_FILE_TYPE_UNKNOWN;
}

const gchar *
gb_project_file_get_icon_name (GbProjectFile *self)
{
  if (gb_project_file_get_is_directory (self))
    return "folder-symbolic";

  return "text-x-generic-symbolic";
}

const gchar *
gb_project_file_get_display_name (GbProjectFile *self)
{
  g_return_val_if_fail (GB_IS_PROJECT_FILE (self), NULL);

  if (self->file_info != NULL)
    return g_file_info_get_display_name (self->file_info);

  return NULL;
}
