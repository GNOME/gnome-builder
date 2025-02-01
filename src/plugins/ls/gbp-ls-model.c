/* gbp-ls-model.c
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

#define G_LOG_DOMAIN "gbp-ls-model"

#include "config.h"

#include <libide-gui.h>

#include "gbp-ls-model.h"

struct _GbpLsModel
{
  GObject    parent_instance;
  GFile     *directory;
  GPtrArray *items;
};

enum {
  PROP_0,
  PROP_DIRECTORY,
  N_PROPS
};

static void async_initable_iface_init (GAsyncInitableIface *iface);
static void tree_model_iface_init     (GtkTreeModelIface   *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpLsModel, gbp_ls_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, tree_model_iface_init))

static GParamSpec *properties [N_PROPS];

static gint
compare_by_name (gconstpointer a,
                 gconstpointer b)
{
  GFileInfo *info_a = *(GFileInfo **)a;
  GFileInfo *info_b = *(GFileInfo **)b;
  GFileType type_a = g_file_info_get_file_type (info_a);
  GFileType type_b = g_file_info_get_file_type (info_b);
  const gchar *display_a;
  const gchar *display_b;

  if (type_a != type_b)
    {
      if (type_a == G_FILE_TYPE_DIRECTORY)
        return -1;
      else if (type_b == G_FILE_TYPE_DIRECTORY)
        return 1;
    }

  display_a = g_file_info_get_display_name (info_a);
  display_b = g_file_info_get_display_name (info_b);

  if (g_strcmp0 (display_a, "..") == 0)
    return -1;
  else if (g_strcmp0 (display_b, "..") == 0)
    return 1;
  else
    return g_utf8_collate (display_a, display_b);
}

static void
gbp_ls_model_finalize (GObject *object)
{
  GbpLsModel *self = (GbpLsModel *)object;

  g_clear_object (&self->directory);
  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_ls_model_parent_class)->finalize (object);
}

static void
gbp_ls_model_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbpLsModel *self = GBP_LS_MODEL (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_object (value, self->directory);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_ls_model_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GbpLsModel *self = GBP_LS_MODEL (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      self->directory = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_ls_model_class_init (GbpLsModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_ls_model_finalize;
  object_class->get_property = gbp_ls_model_get_property;
  object_class->set_property = gbp_ls_model_set_property;

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         "Directory",
                         "The directory to display contents from",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_ls_model_init (GbpLsModel *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

static gint
gbp_ls_model_get_n_columns (GtkTreeModel *model)
{
  return GBP_LS_MODEL_N_COLUMNS;
}

static GType
gbp_ls_model_get_column_type (GtkTreeModel *model,
                              gint          column)
{
  switch (column)
    {
    case GBP_LS_MODEL_COLUMN_GICON:    return G_TYPE_ICON;
    case GBP_LS_MODEL_COLUMN_NAME:     return G_TYPE_STRING;
    case GBP_LS_MODEL_COLUMN_SIZE:     return G_TYPE_INT64;
    case GBP_LS_MODEL_COLUMN_MODIFIED: return G_TYPE_DATE_TIME;
    case GBP_LS_MODEL_COLUMN_FILE:     return G_TYPE_FILE;
    case GBP_LS_MODEL_N_COLUMNS:
    default:
      g_return_val_if_reached (G_TYPE_INVALID);
    }
}

static gboolean
gbp_ls_model_get_iter (GtkTreeModel *model,
                       GtkTreeIter  *iter,
                       GtkTreePath  *path)
{
  GbpLsModel *self = (GbpLsModel *)model;
  gint *indicies;
  gint depth = 0;

  g_assert (GBP_IS_LS_MODEL (model));
  g_assert (iter != NULL);

  indicies = gtk_tree_path_get_indices_with_depth (path, &depth);

  if (depth == 1 && indicies[0] < self->items->len)
    {
      iter->user_data = g_ptr_array_index (self->items, indicies[0]);
      iter->user_data2 = GUINT_TO_POINTER (indicies[0]);
      return TRUE;
    }

  return FALSE;
}

static void
gbp_ls_model_get_value (GtkTreeModel *model,
                        GtkTreeIter  *iter,
                        gint          column,
                        GValue       *value)
{
  GbpLsModel *self = (GbpLsModel *)model;
  GFileInfo *info;

  g_assert (GBP_IS_LS_MODEL (self));
  g_assert (iter != NULL);
  g_assert (value != NULL);

  info = iter->user_data;

  g_assert (G_IS_FILE_INFO (info));

  switch (column)
    {
    case GBP_LS_MODEL_COLUMN_GICON:
      g_value_init (value, G_TYPE_ICON);
      g_value_set_object (value,
                          g_file_info_get_attribute_object (info,
                                                            G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON));
      break;

    case GBP_LS_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, g_file_info_get_display_name (info));
      break;

    case GBP_LS_MODEL_COLUMN_SIZE:
      g_value_init (value, G_TYPE_INT64);
      g_value_set_int64 (value, g_file_info_get_size (info));
      break;

    case GBP_LS_MODEL_COLUMN_MODIFIED:
      g_value_init (value, G_TYPE_DATE_TIME);
      g_value_take_boxed (value, g_file_info_get_modification_date_time (info));
      break;

    case GBP_LS_MODEL_COLUMN_FILE:
      {
        g_value_init (value, G_TYPE_FILE);
        g_value_take_object (value,
                             g_file_get_child (self->directory,
                                               g_file_info_get_name (info)));
        break;
      }

    case GBP_LS_MODEL_COLUMN_TYPE:
      {
        g_value_init (value, G_TYPE_FILE_TYPE);
        g_value_set_enum (value, g_file_info_get_file_type (info));
        break;
      }

    case GBP_LS_MODEL_N_COLUMNS:
    default:
      g_return_if_reached ();
    }
}

static gint
gbp_ls_model_iter_n_children (GtkTreeModel *model,
                              GtkTreeIter  *iter)
{
  GbpLsModel *self = (GbpLsModel *)model;

  g_assert (GBP_IS_LS_MODEL (self));

  return iter ? 0 : self->items->len;
}

static gboolean
gbp_ls_model_iter_children (GtkTreeModel *model,
                            GtkTreeIter  *iter,
                            GtkTreeIter  *parent)
{
  GbpLsModel *self = (GbpLsModel *)model;

  g_assert (GBP_IS_LS_MODEL (self));

  if (iter != NULL || self->items->len == 0)
    return FALSE;

  iter->user_data = g_ptr_array_index (self->items, 0);
  iter->user_data2 = GUINT_TO_POINTER (0);

  return TRUE;
}

static gboolean
gbp_ls_model_iter_next (GtkTreeModel *model,
                        GtkTreeIter  *iter)
{
  GbpLsModel *self = (GbpLsModel *)model;
  guint index_;

  g_assert (GBP_IS_LS_MODEL (self));
  g_assert (iter != NULL);

  index_ = GPOINTER_TO_UINT (iter->user_data2);

  if (index_ != G_MAXUINT && index_ + 1 < self->items->len)
    {
      index_++;

      iter->user_data = g_ptr_array_index (self->items, index_);
      iter->user_data2 = GUINT_TO_POINTER (index_);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gbp_ls_model_iter_nth_child (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             GtkTreeIter  *parent,
                             gint          n)
{
  GbpLsModel *self = (GbpLsModel *)model;

  g_assert (GBP_IS_LS_MODEL (model));
  g_assert (iter != NULL);

  if (parent != NULL)
    return FALSE;

  if (n < self->items->len)
    {
      iter->user_data = g_ptr_array_index (self->items, n);
      iter->user_data2 = GINT_TO_POINTER (n);
      return TRUE;
    }

  return FALSE;
}

static GtkTreePath *
gbp_ls_model_get_path (GtkTreeModel *model,
                       GtkTreeIter  *iter)
{
  g_assert (GBP_IS_LS_MODEL (model));
  g_assert (iter != NULL);

  return gtk_tree_path_new_from_indices (GPOINTER_TO_UINT (iter->user_data2), -1);
}

static gboolean
gbp_ls_model_iter_has_child (GtkTreeModel *model,
                             GtkTreeIter  *parent)
{
  GbpLsModel *self = (GbpLsModel *)model;

  g_assert (GBP_IS_LS_MODEL (self));

  return parent ? 0 : self->items->len > 0;
}

static void
tree_model_iface_init (GtkTreeModelIface *iface)
{
  iface->get_n_columns = gbp_ls_model_get_n_columns;
  iface->get_iter = gbp_ls_model_get_iter;
  iface->get_column_type = gbp_ls_model_get_column_type;
  iface->get_value = gbp_ls_model_get_value;
  iface->iter_n_children = gbp_ls_model_iter_n_children;
  iface->iter_children = gbp_ls_model_iter_children;
  iface->iter_next = gbp_ls_model_iter_next;
  iface->iter_nth_child = gbp_ls_model_iter_nth_child;
  iface->iter_has_child = gbp_ls_model_iter_has_child;
  iface->get_path = gbp_ls_model_get_path;
}

static void
gbp_ls_model_worker (IdeTask      *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GPtrArray) items = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GIcon) icon = NULL;
  g_autoptr(GFile) parent = NULL;
  GFile *directory = task_data;
  GFileInfo *info;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
                                          G_FILE_ATTRIBUTE_STANDARD_SIZE","
                                          G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                          G_FILE_ATTRIBUTE_TIME_MODIFIED","
                                          G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC",",
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          &error);

  if (enumerator == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Prefer folder-symbolic over queried icon */
  icon = g_themed_icon_new ("folder-symbolic");

  items = g_ptr_array_new_with_free_func (g_object_unref);

  if ((parent = g_file_get_parent (directory)))
    {
      g_autoptr(GFileInfo) dot = NULL;

      dot = g_file_query_info (parent,
                               G_FILE_ATTRIBUTE_STANDARD_SIZE","
                               G_FILE_ATTRIBUTE_TIME_MODIFIED","
                               G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC",",
                               G_FILE_QUERY_INFO_NONE,
                               cancellable,
                               NULL);

      if (dot != NULL)
        {
          g_file_info_set_name (dot, "..");
          g_file_info_set_display_name (dot, "..");
          g_file_info_set_file_type (dot, G_FILE_TYPE_DIRECTORY);
          g_file_info_set_attribute_object (dot,
                                            G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON,
                                            G_OBJECT (icon));
          g_ptr_array_add (items, g_steal_pointer (&dot));
        }
    }

  while ((info = g_file_enumerator_next_file (enumerator, cancellable, &error)))
    {
      GFileType file_type = g_file_info_get_file_type (info);
      g_autoptr(GIcon) file_icon = NULL;

      /* Prefer our symbolic icon for folders */
      if (file_type == G_FILE_TYPE_DIRECTORY)
        file_icon = g_object_ref (icon);
      else
        file_icon = ide_g_content_type_get_symbolic_icon (g_file_info_get_content_type (info),
                                                          g_file_info_get_display_name (info));

      g_file_info_set_attribute_object (info, G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON, G_OBJECT (file_icon));

      g_ptr_array_add (items, info);
    }

  if (error != NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_ptr_array_sort (items, compare_by_name);

  ide_task_return_pointer (task,
                           g_steal_pointer (&items),
                           g_ptr_array_unref);
}

static void
gbp_ls_model_init_async (GAsyncInitable      *initable,
                         gint                 io_priority,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GbpLsModel *self = (GbpLsModel *)initable;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_LS_MODEL (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_ls_model_init_async);
  ide_task_set_priority (task, io_priority);

  if (self->directory == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 "No directory found to enumerate");
      return;
    }

  ide_task_set_task_data (task, g_object_ref (self->directory), g_object_unref);
  ide_task_run_in_thread (task, gbp_ls_model_worker);
}

static gboolean
gbp_ls_model_init_finish (GAsyncInitable  *initable,
                          GAsyncResult    *result,
                          GError         **error)
{
  g_autoptr(GPtrArray) items = NULL;
  GbpLsModel *self = (GbpLsModel *)initable;

  g_assert (GBP_IS_LS_MODEL (initable));
  g_assert (IDE_IS_TASK (result));

  items = ide_task_propagate_pointer (IDE_TASK (result), error);

  if (items != NULL)
    {
      g_clear_pointer (&self->items, g_ptr_array_unref);
      self->items = g_steal_pointer (&items);
      return TRUE;
    }

  return FALSE;
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = gbp_ls_model_init_async;
  iface->init_finish = gbp_ls_model_init_finish;
}

GbpLsModel *
gbp_ls_model_new (GFile *directory)
{
  g_return_val_if_fail (G_IS_FILE (directory), NULL);

  return g_object_new (GBP_TYPE_LS_MODEL,
                       "directory", directory,
                       NULL);
}

GFile *
gbp_ls_model_get_directory (GbpLsModel *self)
{
  g_return_val_if_fail (GBP_IS_LS_MODEL (self), NULL);

  return self->directory;
}
