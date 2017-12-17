/* ide-test-panel.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-test-panel"

#include "ide-context.h"
#include "ide-debug.h"

#include "buildsystem/ide-build-manager.h"
#include "buildsystem/ide-build-pipeline.h"
#include "testing/ide-test.h"
#include "testing/ide-test-manager.h"
#include "testing/ide-test-panel.h"
#include "testing/ide-test-private.h"
#include "util/ide-gtk.h"

struct _IdeTestPanel
{
  GtkBin             parent_instance;

  /* Owned references */
  IdeTestManager    *manager;

  /* Template references */
  GtkScrolledWindow *scroller;
  GtkStack          *stack;
  GtkTreeView       *tree_view;
};

enum {
  PROP_0,
  PROP_MANAGER,
  N_PROPS
};

G_DEFINE_TYPE (IdeTestPanel, ide_test_panel, GTK_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
ide_test_panel_row_activated (IdeTestPanel      *self,
                              GtkTreePath       *path,
                              GtkTreeViewColumn *column,
                              GtkTreeView       *tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_TEST_PANEL (self));
  g_assert (path != NULL);
  g_assert (GTK_IS_TREE_VIEW_COLUMN (column));
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      g_autoptr(IdeTest) test = NULL;

      if (gtk_tree_model_iter_n_children (model, &iter))
        {
          if (gtk_tree_view_row_expanded (self->tree_view, path))
            gtk_tree_view_collapse_row (self->tree_view, path);
          else
            gtk_tree_view_expand_row (self->tree_view, path, TRUE);
          return;
        }

      gtk_tree_model_get (model, &iter,
                          IDE_TEST_COLUMN_TEST, &test,
                          -1);

      if (test != NULL)
        {
          IdeTestProvider *provider = _ide_test_get_provider (test);
          IdeContext *context = ide_widget_get_context (GTK_WIDGET (self));
          IdeBuildManager *build_manager = ide_context_get_build_manager (context);
          IdeBuildPipeline *pipeline = ide_build_manager_get_pipeline (build_manager);

          /* TODO: Everything...
           *
           *   - We need to track output from the test
           *   - We need to track failure/success from the test
           *   - We need to allow the user to jump to the assertion failure if there was one
           *   - We need to allow the user to run the test w/ the debugger
           */

          ide_test_provider_run_async (provider,
                                       test,
                                       pipeline,
                                       NULL,
                                       NULL,
                                       NULL);
        }
    }

  IDE_EXIT;
}

static void
ide_test_panel_pixbuf_cell_data_func (GtkCellLayout   *layout,
                                      GtkCellRenderer *cell,
                                      GtkTreeModel    *model,
                                      GtkTreeIter     *iter,
                                      gpointer         user_data)
{
  IdeTestPanel *self = user_data;
  g_autofree gchar *title = NULL;
  g_autoptr(IdeTest) test = NULL;
  const gchar *icon_name = NULL;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (layout));
  g_assert (GTK_IS_CELL_RENDERER_PIXBUF (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (IDE_IS_TEST_PANEL (self));

  gtk_tree_model_get (model, iter,
                      IDE_TEST_COLUMN_GROUP, &title,
                      IDE_TEST_COLUMN_TEST, &test,
                      -1);

  if (title)
    {
      GtkTreePath *path = gtk_tree_model_get_path (model, iter);

      if (gtk_tree_view_row_expanded (self->tree_view, path))
        g_object_set (cell, "icon-name", "folder-open-symbolic", NULL);
      else
        g_object_set (cell, "icon-name", "folder-symbolic", NULL);

      gtk_tree_path_free (path);

      return;
    }

  icon_name = ide_test_get_icon_name (test);
  g_object_set (cell, "icon-name", icon_name, NULL);
}

static void
ide_test_panel_text_cell_data_func (GtkCellLayout   *layout,
                                    GtkCellRenderer *cell,
                                    GtkTreeModel    *model,
                                    GtkTreeIter     *iter,
                                    gpointer         user_data)
{
  g_autofree gchar *title = NULL;
  g_autoptr(IdeTest) test = NULL;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (IDE_IS_TEST_PANEL (user_data));

  gtk_tree_model_get (model, iter,
                      IDE_TEST_COLUMN_GROUP, &title,
                      IDE_TEST_COLUMN_TEST, &test,
                      -1);

  if (title)
    g_object_set (cell, "text", title, NULL);
  else if (test)
    g_object_set (cell, "text", ide_test_get_display_name (test), NULL);
  else
    g_object_set (cell, "text", NULL, NULL);

  /* TODO: extract test info/failures/etc */
}

static void
ide_test_panel_row_inserted (IdeTestPanel *self,
                             GtkTreePath  *path,
                             GtkTreeIter  *iter,
                             GtkTreeModel *model)
{
  g_assert (IDE_IS_TEST_PANEL (self));
  g_assert (path != NULL);
  g_assert (iter != NULL);
  g_assert (GTK_IS_TREE_MODEL (model));

  if (self->tree_view != NULL)
    gtk_tree_view_expand_to_path (self->tree_view, path);
}

static void
ide_test_panel_notify_loading (IdeTestPanel   *self,
                               GParamSpec     *pspec,
                               IdeTestManager *manager)
{
  g_assert (IDE_IS_TEST_PANEL (self));
  g_assert (IDE_IS_TEST_MANAGER (manager));

  if (ide_test_manager_get_loading (manager))
    gtk_stack_set_visible_child_name (self->stack, "empty");
  else
    gtk_stack_set_visible_child_name (self->stack, "tests");
}

static void
ide_test_panel_constructed (GObject *object)
{
  IdeTestPanel *self = (IdeTestPanel *)object;

  g_assert (IDE_IS_TEST_PANEL (self));

  G_OBJECT_CLASS (ide_test_panel_parent_class)->constructed (object);

  if (self->manager != NULL)
    {
      GtkTreeModel *model;

      model = _ide_test_manager_get_model (self->manager);

      gtk_tree_view_set_model (self->tree_view, model);

      g_signal_connect_object (model,
                               "row-inserted",
                               G_CALLBACK (ide_test_panel_row_inserted),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (self->manager,
                               "notify::loading",
                               G_CALLBACK (ide_test_panel_notify_loading),
                               self,
                               G_CONNECT_SWAPPED);

      ide_test_panel_notify_loading (self, NULL, self->manager);
    }
}

static void
ide_test_panel_destroy (GtkWidget *widget)
{
  IdeTestPanel *self = (IdeTestPanel *)widget;

  g_assert (IDE_IS_TEST_PANEL (self));

  g_clear_object (&self->manager);

  GTK_WIDGET_CLASS (ide_test_panel_parent_class)->destroy (widget);
}

static void
ide_test_panel_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeTestPanel *self = IDE_TEST_PANEL (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_test_panel_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeTestPanel *self = IDE_TEST_PANEL (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      self->manager = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_test_panel_class_init (IdeTestPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_test_panel_constructed;
  object_class->get_property = ide_test_panel_get_property;
  object_class->set_property = ide_test_panel_set_property;

  widget_class->destroy = ide_test_panel_destroy;

  properties [PROP_MANAGER] =
    g_param_spec_object ("manager",
                         "Manager",
                         "The test manager for the panel",
                         IDE_TYPE_TEST_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-test-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTestPanel, scroller);
  gtk_widget_class_bind_template_child (widget_class, IdeTestPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, IdeTestPanel, tree_view);
  gtk_widget_class_bind_template_callback (widget_class, ide_test_panel_row_activated);
}

static void
ide_test_panel_init (IdeTestPanel *self)
{
  GtkCellRenderer *cell;
  GtkTreeViewColumn *column;

  gtk_widget_init_template (GTK_WIDGET (self));

  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "visible", TRUE,
                         NULL);
  cell = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
                       "xpad", 3,
                       NULL);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column),
                                      cell,
                                      ide_test_panel_pixbuf_cell_data_func,
                                      self,
                                      NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, FALSE);
  cell = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                       "ellipsize", PANGO_ELLIPSIZE_END,
                       "xalign", 0.0f,
                       NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column),
                                      cell,
                                      ide_test_panel_text_cell_data_func,
                                      self,
                                      NULL);
  gtk_tree_view_append_column (self->tree_view, column);
}
