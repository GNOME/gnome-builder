/* gbp-todo-panel.c
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

#define G_LOG_DOMAIN "gbp-todo-panel"

#include <glib/gi18n.h>
#include <libide-code.h>
#include <libide-gui.h>

#include "gbp-todo-item.h"
#include "gbp-todo-panel.h"

struct _GbpTodoPanel
{
  DzlDockWidget  parent_instance;

  GbpTodoModel  *model;

  GtkTreeView   *tree_view;
  GtkStack      *stack;
};

G_DEFINE_TYPE (GbpTodoPanel, gbp_todo_panel, DZL_TYPE_DOCK_WIDGET)

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_todo_panel_cell_data_func (GtkCellLayout   *cell_layout,
                               GtkCellRenderer *cell,
                               GtkTreeModel    *tree_model,
                               GtkTreeIter     *iter,
                               gpointer         data)
{
  g_autoptr(GbpTodoItem) item = NULL;
  const gchar *message;

  gtk_tree_model_get (tree_model, iter, 0, &item, -1);

  message = gbp_todo_item_get_line (item, 0);

  if (message != NULL)
    {
      g_autofree gchar *title = NULL;
      const gchar *path;
      guint lineno;

      /*
       * We don't trim the whitespace from lines so that we can keep
       * them in tact when showing tooltips. So we need to truncate
       * here for display in the pane.
       */
      while (g_ascii_isspace (*message))
        message++;

      path = gbp_todo_item_get_path (item);
      lineno = gbp_todo_item_get_lineno (item);
      title = g_strdup_printf ("%s:%u", path, lineno);
      ide_cell_renderer_fancy_take_title (IDE_CELL_RENDERER_FANCY (cell),
                                          g_steal_pointer (&title));
      ide_cell_renderer_fancy_set_body (IDE_CELL_RENDERER_FANCY (cell), message);
    }
  else
    {
      ide_cell_renderer_fancy_set_body (IDE_CELL_RENDERER_FANCY (cell), NULL);
      ide_cell_renderer_fancy_set_title (IDE_CELL_RENDERER_FANCY (cell), NULL);
    }
}

static void
gbp_todo_panel_row_activated (GbpTodoPanel      *self,
                              GtkTreePath       *tree_path,
                              GtkTreeViewColumn *column,
                              GtkTreeView       *tree_view)
{
  g_autoptr(GbpTodoItem) item = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *fragment = NULL;
  IdeWorkbench *workbench;
  GtkTreeModel *model;
  const gchar *path;
  GtkTreeIter iter;
  guint lineno;

  g_assert (GBP_IS_TODO_PANEL (self));
  g_assert (tree_path != NULL);
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  model = gtk_tree_view_get_model (tree_view);
  gtk_tree_model_get_iter (model, &iter, tree_path);
  gtk_tree_model_get (model, &iter, 0, &item, -1);
  g_assert (GBP_IS_TODO_ITEM (item));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  path = gbp_todo_item_get_path (item);
  g_assert (path != NULL);

  if (g_path_is_absolute (path))
    {
      file = g_file_new_for_path (path);
    }
  else
    {
      IdeContext *context;
      IdeVcs *vcs;
      GFile *workdir;

      context = ide_workbench_get_context (workbench);
      vcs = ide_vcs_from_context (context);
      workdir = ide_vcs_get_workdir (vcs);
      file = g_file_get_child (workdir, path);
    }

  /* Set lineno info so that the editor can jump to the location of the TODO
   * item. Our line number from the model is 1-based, and we need 0-based for
   * our API to open files.
   */
  lineno = gbp_todo_item_get_lineno (item);
  if (lineno > 0)
    lineno--;

  ide_workbench_open_at_async (workbench,
                               file,
                               "editor",
                               lineno,
                               -1,
                               IDE_BUFFER_OPEN_FLAGS_NONE,
                               NULL, NULL, NULL);
}

static gboolean
gbp_todo_panel_query_tooltip (GbpTodoPanel *self,
                              gint          x,
                              gint          y,
                              gboolean      keyboard_mode,
                              GtkTooltip   *tooltip,
                              GtkTreeView  *tree_view)
{
  GtkTreePath *path = NULL;
  GtkTreeModel *model;

  g_assert (GBP_IS_TODO_PANEL (self));
  g_assert (GTK_IS_TOOLTIP (tooltip));
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  if (NULL == (model = gtk_tree_view_get_model (tree_view)))
    return FALSE;

  if (gtk_tree_view_get_path_at_pos (tree_view, x, y, &path, NULL, NULL, NULL))
    {
      GtkTreeIter iter;

      if (gtk_tree_model_get_iter (model, &iter, path))
        {
          g_autoptr(GbpTodoItem) item = NULL;
          g_autoptr(GString) str = g_string_new ("<tt>");

          gtk_tree_model_get (model, &iter, 0, &item, -1);
          g_assert (GBP_IS_TODO_ITEM (item));

          /* only 5 lines stashed */
          for (guint i = 0; i < 5; i++)
            {
              const gchar *line = gbp_todo_item_get_line (item, i);
              g_autofree gchar *escaped = NULL;

              if (!line)
                break;

              escaped = g_markup_escape_text (line, -1);
              g_string_append (str, escaped);
              g_string_append_c (str, '\n');
            }

          g_string_append (str, "</tt>");
          gtk_tooltip_set_markup (tooltip, str->str);
        }

      gtk_tree_path_free (path);

      return TRUE;
    }

  return FALSE;
}

static void
gbp_todo_panel_destroy (GtkWidget *widget)
{
  GbpTodoPanel *self = (GbpTodoPanel *)widget;

  g_assert (GBP_IS_TODO_PANEL (self));

  if (self->tree_view != NULL)
    gtk_tree_view_set_model (self->tree_view, NULL);

  g_clear_object (&self->model);

  GTK_WIDGET_CLASS (gbp_todo_panel_parent_class)->destroy (widget);
}

static void
gbp_todo_panel_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpTodoPanel *self = GBP_TODO_PANEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, gbp_todo_panel_get_model (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_todo_panel_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbpTodoPanel *self = GBP_TODO_PANEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gbp_todo_panel_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_todo_panel_class_init (GbpTodoPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = gbp_todo_panel_get_property;
  object_class->set_property = gbp_todo_panel_set_property;

  widget_class->destroy = gbp_todo_panel_destroy;

  properties [PROP_MODEL] =
    g_param_spec_object ("model",
                         "Model",
                         "The model for the TODO list",
                         GBP_TYPE_TODO_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_todo_panel_init (GbpTodoPanel *self)
{
  GtkWidget *scroller;
  GtkWidget *empty;
  GtkTreeSelection *selection;

  self->stack = g_object_new (GTK_TYPE_STACK,
                              "transition-duration", 333,
                              "transition-type", GTK_STACK_TRANSITION_TYPE_CROSSFADE,
                              "homogeneous", FALSE,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->stack));

  empty = g_object_new (DZL_TYPE_EMPTY_STATE,
                        "title", _("Loading TODOs…"),
                        "subtitle", _("Please wait while we scan your project"),
                        "icon-name", "emblem-ok-symbolic",
                        "valign", GTK_ALIGN_START,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (self->stack), empty);

  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "visible", TRUE,
                           "vexpand", TRUE,
                           NULL);
  gtk_container_add_with_properties (GTK_CONTAINER (self->stack), scroller,
                                     "name", "todos",
                                     NULL);

  self->tree_view = g_object_new (IDE_TYPE_FANCY_TREE_VIEW,
                                  "has-tooltip", TRUE,
                                  "visible", TRUE,
                                  NULL);
  g_signal_connect (self->tree_view,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->tree_view);
  g_signal_connect_swapped (self->tree_view,
                            "row-activated",
                            G_CALLBACK (gbp_todo_panel_row_activated),
                            self);
  g_signal_connect_swapped (self->tree_view,
                            "query-tooltip",
                            G_CALLBACK (gbp_todo_panel_query_tooltip),
                            self);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->tree_view), "i-wanna-be-listbox");
  gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (self->tree_view));

  selection = gtk_tree_view_get_selection (self->tree_view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);

  ide_fancy_tree_view_set_data_func (IDE_FANCY_TREE_VIEW (self->tree_view),
                                     gbp_todo_panel_cell_data_func, NULL, NULL);
}

/**
 * gbp_todo_panel_get_model:
 * @self: a #GbpTodoPanel
 *
 * Gets the model being displayed by the treeview.
 *
 * Returns: (transfer none) (nullable): a #GbpTodoModel.
 *
 * Since: 3.32
 */
GbpTodoModel *
gbp_todo_panel_get_model (GbpTodoPanel *self)
{
  g_return_val_if_fail (GBP_IS_TODO_PANEL (self), NULL);

  return self->model;
}

void
gbp_todo_panel_set_model (GbpTodoPanel *self,
                          GbpTodoModel *model)
{
  g_return_if_fail (GBP_IS_TODO_PANEL (self));
  g_return_if_fail (!model || GBP_IS_TODO_MODEL (model));

  if (g_set_object (&self->model, model))
    {
      if (self->model != NULL)
        gtk_tree_view_set_model (self->tree_view, GTK_TREE_MODEL (self->model));
      else
        gtk_tree_view_set_model (self->tree_view, NULL);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODEL]);
    }
}

void
gbp_todo_panel_make_ready (GbpTodoPanel *self)
{
  g_return_if_fail (GBP_IS_TODO_PANEL (self));

  gtk_stack_set_visible_child_name (self->stack, "todos");
}
