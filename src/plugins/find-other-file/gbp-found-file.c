/* gbp-found-file.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-found-file"

#include "config.h"

#include "gbp-found-file.h"

struct _GbpFoundFile
{
  GObject parent_instance;
  GFile *file;
  GFileInfo *info;
  char *relative;
};

enum {
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_FILE,
  PROP_GICON,
  PROP_IS_DIRECTORY,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpFoundFile, gbp_found_file, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static gboolean
gbp_found_file_get_is_directory (GbpFoundFile *self)
{
  g_assert (GBP_IS_FOUND_FILE (self));

  return self->info != NULL ?
         g_file_info_get_file_type (self->info) == G_FILE_TYPE_DIRECTORY :
         FALSE;
}

static void
gbp_found_file_query_info_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GbpFoundFile) self = user_data;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_FOUND_FILE (self));

  if (!(self->info = g_file_query_info_finish (file, result, NULL)))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_GICON]);

  if (gbp_found_file_get_is_directory (self))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_DIRECTORY]);
}

static void
gbp_found_file_constructed (GObject *object)
{
  GbpFoundFile *self = (GbpFoundFile *)object;

  G_OBJECT_CLASS (gbp_found_file_parent_class)->constructed (object);

  if (self->file == NULL)
    return;

  g_file_query_info_async (self->file,
                           G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                           G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON","
                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_LOW,
                           NULL,
                           gbp_found_file_query_info_cb,
                           g_object_ref (self));
}

static void
gbp_found_file_dispose (GObject *object)
{
  GbpFoundFile *self = (GbpFoundFile *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->info);
  g_clear_pointer (&self->relative, g_free);

  G_OBJECT_CLASS (gbp_found_file_parent_class)->dispose (object);
}

static void
gbp_found_file_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpFoundFile *self = GBP_FOUND_FILE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      if (self->relative != NULL)
        g_value_set_string (value, self->relative);
      else if (self->info != NULL)
        g_value_set_string (value, g_file_info_get_attribute_string (self->info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME));
      break;

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_GICON:
      if (self->info != NULL)
        g_value_set_object (value, g_file_info_get_attribute_object (self->info, G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON));
      break;

    case PROP_IS_DIRECTORY:
      g_value_set_boolean (value, gbp_found_file_get_is_directory (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_found_file_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbpFoundFile *self = GBP_FOUND_FILE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      self->file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_found_file_class_init (GbpFoundFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_found_file_constructed;
  object_class->dispose = gbp_found_file_dispose;
  object_class->get_property = gbp_found_file_get_property;
  object_class->set_property = gbp_found_file_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_GICON] =
    g_param_spec_object ("gicon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_IS_DIRECTORY] =
    g_param_spec_boolean ("is-directory", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_found_file_init (GbpFoundFile *self)
{
}

GbpFoundFile *
gbp_found_file_new (GFile *workdir,
                    GFile *file)
{
  GbpFoundFile *ret;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  ret = g_object_new (GBP_TYPE_FOUND_FILE,
                      "file", file,
                      NULL);
  ret->relative = g_file_get_relative_path (workdir, file);

  return ret;
}

void
gbp_found_file_open (GbpFoundFile *self,
                     IdeWorkspace *workspace)
{
  g_autoptr(PanelPosition) position = NULL;
  IdeWorkbench *workbench;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_FOUND_FILE (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  workbench = ide_workspace_get_workbench (workspace);
  position = panel_position_new ();

  ide_workbench_open_async (workbench,
                            self->file,
                            NULL,
                            IDE_BUFFER_OPEN_FLAGS_NONE,
                            position,
                            NULL, NULL, NULL);

  IDE_EXIT;
}
