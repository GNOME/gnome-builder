/* ide-project-file.c
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

#define G_LOG_DOMAIN "ide-project-file"

#include "config.h"

#include <libide-threading.h>
#include <string.h>

#include "ide-project.h"
#include "ide-project-file.h"

typedef struct
{
  GFile     *directory;
  GFileInfo *info;
  guint      checked_for_icon_override : 1;
} IdeProjectFilePrivate;

enum {
  PROP_0,
  PROP_DIRECTORY,
  PROP_FILE,
  PROP_INFO,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeProjectFile, ide_project_file, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static gchar *
ide_project_file_repr (IdeObject *object)
{
  IdeProjectFile *self = (IdeProjectFile *)object;
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_assert (IDE_IS_PROJECT_FILE (self));

  if (priv->info && priv->directory)
    return g_strdup_printf ("%s name=\"%s\" directory=\"%s\"",
                            G_OBJECT_TYPE_NAME (self),
                            g_file_info_get_display_name (priv->info),
                            g_file_peek_path (priv->directory));
  else
    return IDE_OBJECT_CLASS (ide_project_file_parent_class)->repr (object);
}

static void
ide_project_file_destroy (IdeObject *object)
{
  IdeProjectFile *self = (IdeProjectFile *)object;
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_clear_object (&priv->directory);
  g_clear_object (&priv->info);

  IDE_OBJECT_CLASS (ide_project_file_parent_class)->destroy (object);
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
    case PROP_DIRECTORY:
      g_value_set_object (value, ide_project_file_get_directory (self));
      break;

    case PROP_FILE:
      g_value_take_object (value, ide_project_file_ref_file (self));
      break;

    case PROP_INFO:
      g_value_set_object (value, ide_project_file_get_info (self));
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
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      priv->directory = g_value_dup_object (value);
      break;

    case PROP_INFO:
      priv->info = g_value_dup_object (value);
      if (priv->info &&
          g_file_info_has_attribute (priv->info, G_FILE_ATTRIBUTE_STANDARD_NAME) &&
          g_file_info_has_attribute (priv->info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
        break;
      /* Fall-through */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_file_class_init (IdeProjectFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_project_file_get_property;
  object_class->set_property = ide_project_file_set_property;

  i_object_class->destroy = ide_project_file_destroy;
  i_object_class->repr = ide_project_file_repr;

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         "Directory",
                         "The directory containing the file",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_INFO] =
    g_param_spec_object ("info",
                         "Info",
                         "The file info the file",
                         G_TYPE_FILE_INFO,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The file",
                         G_TYPE_FILE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_project_file_init (IdeProjectFile *self)
{
}

/**
 * ide_project_file_get_directory:
 * @self: a #IdeProjectFile
 *
 * Gets the project file.
 *
 * Returns: (transfer none): an #IdeProjectFile
 */
GFile *
ide_project_file_get_directory (IdeProjectFile *self)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), NULL);

  return priv->directory;
}

/**
 * ide_project_file_ref_file:
 * @self: a #IdeProjectFile
 *
 * Gets the file for the #IdeProjectFile.
 *
 * Returns: (transfer full): a #GFile
 */
GFile *
ide_project_file_ref_file (IdeProjectFile *self)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), NULL);

  return g_file_get_child (priv->directory, g_file_info_get_name (priv->info));
}

/**
 * ide_project_file_get_info:
 * @self: a #IdeProjectFile
 *
 * Gets the #GFileInfo for the file. This combined with
 * #IdeProjectFile:directory can be used to determine the underlying
 * file, such as via #IdeProjectFile:file.
 *
 * Returns: (transfer none): a #GFileInfo
 */
GFileInfo *
ide_project_file_get_info (IdeProjectFile *self)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), NULL);

  return priv->info;
}

/**
 * ide_project_file_get_name:
 * @self: a #IdeProjectFile
 *
 * Gets the name for the file, which matches the encoding on disk.
 *
 * Returns: a string containing the name
 */
const gchar *
ide_project_file_get_name (IdeProjectFile *self)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), NULL);

  return g_file_info_get_name (priv->info);
}

/**
 * ide_project_file_get_display_name:
 * @self: a #IdeProjectFile
 *
 * Gets the display-name for the file, which should be shown to users.
 *
 * Returns: a string containing the display name
 */
const gchar *
ide_project_file_get_display_name (IdeProjectFile *self)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), NULL);

  return g_file_info_get_display_name (priv->info);
}

/**
 * ide_project_file_is_directory:
 * @self: a #IdeProjectFile
 *
 * Checks if @self represents a directory. If ide_project_file_is_symlink() is
 * %TRUE, this may still return %TRUE.
 *
 * Returns: %TRUE if @self is a directory, or symlink to a directory
 */
gboolean
ide_project_file_is_directory (IdeProjectFile *self)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), FALSE);

  return g_file_info_get_file_type (priv->info) == G_FILE_TYPE_DIRECTORY;
}


/**
 * ide_project_file_is_symlink:
 * @self: a #IdeProjectFile
 *
 * Checks if @self represents a symlink.
 *
 * Returns: %TRUE if @self is a symlink
 */
gboolean
ide_project_file_is_symlink (IdeProjectFile *self)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), FALSE);

  return g_file_info_get_is_symlink (priv->info);
}

gint
ide_project_file_compare (IdeProjectFile *a,
                          IdeProjectFile *b)
{
  GFileInfo *info_a = ide_project_file_get_info (a);
  GFileInfo *info_b = ide_project_file_get_info (b);
  const gchar *display_name_a = g_file_info_get_display_name (info_a);
  const gchar *display_name_b = g_file_info_get_display_name (info_b);
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
ide_project_file_compare_directories_first (IdeProjectFile *a,
                                            IdeProjectFile *b)
{
  GFileInfo *info_a = ide_project_file_get_info (a);
  GFileInfo *info_b = ide_project_file_get_info (b);
  GFileType file_type_a = g_file_info_get_file_type (info_a);
  GFileType file_type_b = g_file_info_get_file_type (info_b);
  gint dir_a = (file_type_a == G_FILE_TYPE_DIRECTORY);
  gint dir_b = (file_type_b == G_FILE_TYPE_DIRECTORY);
  gint ret;

  ret = dir_b - dir_a;
  if (ret == 0)
    ret = ide_project_file_compare (a, b);

  return ret;
}

/**
 * ide_project_file_get_symbolic_icon:
 * @self: a #IdeProjectFile
 *
 * Gets the symbolic icon to represent the file.
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 */
GIcon *
ide_project_file_get_symbolic_icon (IdeProjectFile *self)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), NULL);

  /*
   * We might want to override the symbolic icon based on an override
   * icon we ship with Builder.
   */
  if (!priv->checked_for_icon_override)
    {
      const gchar *content_type;

      priv->checked_for_icon_override = TRUE;

      if ((content_type = g_file_info_get_content_type (priv->info)))
        {
          g_autoptr(GIcon) override = NULL;

          if ((override = ide_g_content_type_get_symbolic_icon (content_type, g_file_info_get_display_name (priv->info))))
            g_file_info_set_symbolic_icon (priv->info, override);
        }
    }

  return g_file_info_get_symbolic_icon (priv->info);
}

static void
ide_project_file_list_children_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GFile *parent = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) files = NULL;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_FILE (parent));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(files = ide_g_file_get_children_finish (parent, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (files, g_object_unref);

  ret = g_ptr_array_new_full (files->len, g_object_unref);

  for (guint i = 0; i < files->len; i++)
    {
      GFileInfo *info = g_ptr_array_index (files, i);
      IdeProjectFile *project_file;

      project_file = g_object_new (IDE_TYPE_PROJECT_FILE,
                                   "info", info,
                                   "directory", parent,
                                   NULL);
      g_ptr_array_add (ret, g_steal_pointer (&project_file));
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           g_ptr_array_unref);
}

/**
 * ide_project_file_list_children_async:
 * @self: a #IdeProjectFile
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: callback to execute upon completion
 * @user_data: user data for @callback
 *
 * List the children of @self.
 *
 * Call ide_project_file_list_children_finish() to get the result
 * of this operation.
 */
void
ide_project_file_list_children_async (IdeProjectFile      *self,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (IDE_IS_PROJECT_FILE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_project_file_list_children_async);

  file = ide_project_file_ref_file (self);

  ide_g_file_get_children_async (file,
                                 IDE_PROJECT_FILE_ATTRIBUTES,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT,
                                 cancellable,
                                 ide_project_file_list_children_cb,
                                 g_steal_pointer (&task));
}

/**
 * ide_project_file_list_children_finish:
 * @self: a #IdeProjectFile
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous request to
 * ide_project_file_list_children_async().
 *
 * Returns: (transfer full) (element-type IdeProjectFile): a #GPtrArray
 *   of #IdeProjectFile
 */
GPtrArray *
ide_project_file_list_children_finish (IdeProjectFile  *self,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  GPtrArray *ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
ide_project_file_trash_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  IdeProject *project = (IdeProject *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_PROJECT (project));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_project_trash_file_finish (project, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

void
ide_project_file_trash_async (IdeProjectFile      *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeProject) project = NULL;
  g_autoptr(GFile) file = NULL;

  g_return_if_fail (IDE_IS_PROJECT_FILE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_project_file_trash_async);

  context = ide_object_ref_context (IDE_OBJECT (self));
  project = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_PROJECT);
  file = ide_project_file_ref_file (self);

  ide_project_trash_file_async (project,
                                file,
                                cancellable,
                                ide_project_file_trash_cb,
                                g_steal_pointer (&task));
}

gboolean
ide_project_file_trash_finish (IdeProjectFile  *self,
                               GAsyncResult    *result,
                               GError         **error)
{
  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

/**
 * ide_project_file_create_child:
 * @self: a #IdeProjectFile
 * @info: a #GFileInfo
 *
 * Creates a new child project file of @self.
 *
 * Returns: (transfer full): an #IdeProjectFile
 */
IdeProjectFile *
ide_project_file_create_child (IdeProjectFile *self,
                               GFileInfo      *info)
{
  IdeProjectFilePrivate *priv = ide_project_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_FILE (self), NULL);
  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  return g_object_new (IDE_TYPE_PROJECT_FILE,
                       "directory", priv->directory,
                       "info", info,
                       NULL);
}

/**
 * ide_project_file_new:
 * @directory: a #GFile
 * @info: a #GFileInfo
 *
 * Creates a new project file for a child of @directory.
 *
 * Returns: (transfer full): an #IdeProjectFile
 */
IdeProjectFile *
ide_project_file_new (GFile     *directory,
                      GFileInfo *info)
{
  g_return_val_if_fail (G_IS_FILE (directory), NULL);
  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  return g_object_new (IDE_TYPE_PROJECT_FILE,
                       "directory", directory,
                       "info", info,
                       NULL);
}
