/* gbp-ls-view.c
 *
 * Copyright © 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "gbp-ls-view"

#include <glib/gi18n.h>

#include "gbp-ls-model.h"
#include "gbp-ls-view.h"

struct _GbpLsView
{
  IdeLayoutView      parent_instance;

  GCancellable      *model_cancellable;
  GbpLsModel        *model;

  GtkScrolledWindow *scroller;
  GtkTreeView       *tree_view;
  GtkTreeViewColumn *modified_column;
  GtkCellRenderer   *modified_cell;
  GtkTreeViewColumn *size_column;
  GtkCellRenderer   *size_cell;

  guint              close_on_activate : 1;
};

enum {
  PROP_0,
  PROP_CLOSE_ON_ACTIVATE,
  PROP_DIRECTORY,
  N_PROPS
};

G_DEFINE_TYPE (GbpLsView, gbp_ls_view, IDE_TYPE_LAYOUT_VIEW)

static GParamSpec *properties [N_PROPS];

static void
gbp_ls_view_row_activated_cb (GbpLsView         *self,
                              GtkTreePath       *path,
                              GtkTreeViewColumn *column,
                              GtkTreeView       *tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (GBP_IS_LS_VIEW (self));
  g_assert (path != NULL);

  if ((model = gtk_tree_view_get_model (tree_view)) &&
      gtk_tree_model_get_iter (model, &iter, path))
    {
      g_autoptr(GFile) file = NULL;
      GFileType file_type;

      gtk_tree_model_get (model, &iter,
                          GBP_LS_MODEL_COLUMN_FILE, &file,
                          GBP_LS_MODEL_COLUMN_TYPE, &file_type,
                          -1);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        gbp_ls_view_set_directory (self, file);
      else
        {
          g_autoptr(IdeUri) uri = ide_uri_new_from_file (file);
          IdeWorkbench *workbench = ide_widget_get_workbench (GTK_WIDGET (self));

          ide_workbench_open_uri_async (workbench,
                                        uri,
                                        NULL,
                                        IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                        NULL,
                                        NULL,
                                        NULL);

          if (self->close_on_activate)
            dzl_gtk_widget_action (GTK_WIDGET (self), "layoutstack", "close-view", NULL);
        }
    }
}

static void
modified_cell_data_func (GtkCellLayout   *cell_layout,
                         GtkCellRenderer *cell,
                         GtkTreeModel    *tree_model,
                         GtkTreeIter     *iter,
                         gpointer         data)
{
  g_autofree gchar *format = NULL;
  g_autoptr(GDateTime) when = NULL;

  gtk_tree_model_get (tree_model, iter,
                      GBP_LS_MODEL_COLUMN_MODIFIED, &when,
                      -1);
  format = dzl_g_date_time_format_for_display (when);
  g_object_set (cell, "text", format, NULL);
}

static void
size_cell_data_func (GtkCellLayout   *cell_layout,
                     GtkCellRenderer *cell,
                     GtkTreeModel    *tree_model,
                     GtkTreeIter     *iter,
                     gpointer         data)
{
  g_autofree gchar *format = NULL;
  gint64 size = -1;

  gtk_tree_model_get (tree_model, iter,
                      GBP_LS_MODEL_COLUMN_SIZE, &size,
                      -1);
  format = g_format_size (size);
  g_object_set (cell, "text", format, NULL);
}

static void
gbp_ls_view_finalize (GObject *object)
{
  GbpLsView *self = (GbpLsView *)object;

  g_clear_object (&self->model_cancellable);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (gbp_ls_view_parent_class)->finalize (object);
}

static void
gbp_ls_view_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GbpLsView *self = GBP_LS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_object (value, gbp_ls_view_get_directory (self));
      break;

    case PROP_CLOSE_ON_ACTIVATE:
      g_value_set_boolean (value, self->close_on_activate);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_ls_view_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GbpLsView *self = GBP_LS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      gbp_ls_view_set_directory (self, g_value_get_object (value));
      break;

    case PROP_CLOSE_ON_ACTIVATE:
      self->close_on_activate = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_ls_view_class_init (GbpLsViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_ls_view_finalize;
  object_class->get_property = gbp_ls_view_get_property;
  object_class->set_property = gbp_ls_view_set_property;

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         "Directory",
                         "The directory to be displayed",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CLOSE_ON_ACTIVATE] =
    g_param_spec_boolean ("close-on-activate", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/ls/gbp-ls-view.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpLsView, modified_cell);
  gtk_widget_class_bind_template_child (widget_class, GbpLsView, modified_column);
  gtk_widget_class_bind_template_child (widget_class, GbpLsView, size_cell);
  gtk_widget_class_bind_template_child (widget_class, GbpLsView, size_column);
  gtk_widget_class_bind_template_child (widget_class, GbpLsView, scroller);
  gtk_widget_class_bind_template_child (widget_class, GbpLsView, tree_view);
}

static void
gbp_ls_view_init (GbpLsView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  ide_layout_view_set_icon_name (IDE_LAYOUT_VIEW (self), "folder-symbolic");

  g_signal_connect_object (self->tree_view,
                           "row-activated",
                           G_CALLBACK (gbp_ls_view_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->size_column),
                                      self->size_cell,
                                      size_cell_data_func,
                                      NULL, NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->modified_column),
                                      self->modified_cell,
                                      modified_cell_data_func,
                                      NULL, NULL);
}

GtkWidget *
gbp_ls_view_new (void)
{
  return g_object_new (GBP_TYPE_LS_VIEW, NULL);
}

GFile *
gbp_ls_view_get_directory (GbpLsView *self)
{
  g_return_val_if_fail (GBP_IS_LS_VIEW (self), NULL);

  if (self->model != NULL)
    return gbp_ls_model_get_directory (GBP_LS_MODEL (self->model));

  return NULL;
}

static void
gbp_ls_view_init_model_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GbpLsModel *model = (GbpLsModel *)object;
  g_autoptr(GbpLsView) self = user_data;
  g_autoptr(GError) error = NULL;
  GtkTreeIter iter;

  g_assert (GBP_IS_LS_MODEL (model));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_LS_VIEW (self));

  if (model != self->model)
    return;

  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (model), result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        ide_layout_view_report_error (IDE_LAYOUT_VIEW (self),
                                      _("Failed to load directory: %s"),
                                      error->message);
      return;
    }

  gtk_tree_view_set_model (self->tree_view, GTK_TREE_MODEL (model));

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter))
    {
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->tree_view));
      gtk_tree_selection_select_iter (selection, &iter);
    }

  gtk_widget_grab_focus (GTK_WIDGET (self->tree_view));
}

void
gbp_ls_view_set_directory (GbpLsView *self,
                           GFile     *directory)
{
  g_autofree gchar *title = NULL;
  g_autofree gchar *name = NULL;
  GFile *old_directory;

  g_return_if_fail (GBP_IS_LS_VIEW (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));

  if (directory == NULL)
    {
      IdeContext *context = ide_widget_get_context (GTK_WIDGET (self));
      IdeVcs *vcs = ide_context_get_vcs (context);

      directory = ide_vcs_get_working_directory (vcs);
    }

  g_assert (directory != NULL);

  old_directory = gbp_ls_view_get_directory (self);

  if (directory != NULL &&
      old_directory != NULL &&
      g_file_equal (directory, old_directory))
    return;

  g_clear_object (&self->model);

  g_cancellable_cancel (self->model_cancellable);
  g_clear_object (&self->model_cancellable);

  self->model_cancellable = g_cancellable_new ();
  self->model = gbp_ls_model_new (directory);

  g_async_initable_init_async (G_ASYNC_INITABLE (self->model),
                               G_PRIORITY_DEFAULT,
                               self->model_cancellable,
                               gbp_ls_view_init_model_cb,
                               g_object_ref (self));

  name = g_file_get_basename (directory);
  title = g_strdup_printf (_("%s — Directory"), name);
  ide_layout_view_set_title (IDE_LAYOUT_VIEW (self), title);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTORY]);
}
