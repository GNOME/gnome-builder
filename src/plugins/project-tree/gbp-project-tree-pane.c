/* gbp-project-tree-pane.c
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

#define G_LOG_DOMAIN "gbp-project-tree-pane"

#include "config.h"

#include <libide-gui.h>
#include <libide-search.h>

#include "gbp-project-tree-private.h"
#include "gbp-project-tree.h"

G_DEFINE_FINAL_TYPE (GbpProjectTreePane, gbp_project_tree_pane, IDE_TYPE_PANE)

static void
gbp_project_tree_pane_search_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeSearchEngine *engine = (IdeSearchEngine *)object;
  g_autoptr(GbpProjectTreePane) self = user_data;
  g_autoptr(IdeSearchResults) model = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_SEARCH_ENGINE (engine));

  model = ide_search_engine_search_finish (engine, result, &error);

  gtk_single_selection_set_model (self->selection, G_LIST_MODEL (model));

  IDE_EXIT;
}

static void
on_search_cb (GbpProjectTreePane *self,
              GtkSearchEntry     *search)
{
  IdeSearchEngine *engine;
  IdeWorkbench *workbench;
  GListModel *model;
  const char *visible_child_name;
  const char *text;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search));

  text = gtk_editable_get_text (GTK_EDITABLE (search));

  if (ide_str_empty0 (text))
    visible_child_name = "tree";
  else
    visible_child_name = "results";

  if (text[0] == 0)
    gtk_single_selection_set_model (self->selection, NULL);

  model = gtk_single_selection_get_model (self->selection);

  if (IDE_IS_SEARCH_RESULTS (model) &&
      ide_search_results_refilter (IDE_SEARCH_RESULTS (model), text))
    goto skip_search;

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  engine = ide_workbench_get_search_engine (workbench);

  g_cancellable_cancel (self->cancellable);
  self->cancellable = g_cancellable_new ();

  ide_search_engine_search_async (engine,
                                  IDE_SEARCH_CATEGORY_FILES,
                                  text,
                                  G_MAXUINT,
                                  self->cancellable,
                                  gbp_project_tree_pane_search_cb,
                                  g_object_ref (self));

skip_search:
  gtk_stack_set_visible_child_name (self->stack, visible_child_name);

  IDE_EXIT;
}

static void
on_list_activate_cb (GbpProjectTreePane *self,
                     guint               position,
                     GtkListView        *list_view)
{
  g_autoptr(IdeSearchResult) result = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  result = g_list_model_get_item (G_LIST_MODEL (self->selection), position);

  ide_search_result_activate (result, GTK_WIDGET (self));

  IDE_EXIT;
}

static void
on_search_activate_cb (GbpProjectTreePane *self,
                       GtkSearchEntry     *search)
{
  IdeSearchResult *result;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_PANE (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search));

  if ((result = gtk_single_selection_get_selected_item (self->selection)))
    ide_search_result_activate (result, GTK_WIDGET (self));

  IDE_EXIT;
}

static gboolean
gbp_project_tree_pane_grab_focus (GtkWidget *widget)
{
  return gtk_widget_grab_focus (GTK_WIDGET (GBP_PROJECT_TREE_PANE (widget)->tree));
}

static void
gbp_project_tree_pane_root (GtkWidget *widget)
{
  GbpProjectTreePane *self = (GbpProjectTreePane *)widget;
  IdeContext *context;

  g_assert (GBP_IS_PROJECT_TREE_PANE (self));

  GTK_WIDGET_CLASS (gbp_project_tree_pane_parent_class)->root (widget);

  /* Only show "Filter" if we have a project, as we need project
   * indexes currently to perform search.
   */
  if (!(context = ide_widget_get_context (widget)) || !ide_context_has_project (context))
    gtk_widget_hide (GTK_WIDGET (self->search));
}

static void
gbp_project_tree_pane_dispose (GObject *object)
{
  GbpProjectTreePane *self = (GbpProjectTreePane *)object;

  g_clear_object (&self->actions);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (gbp_project_tree_pane_parent_class)->dispose (object);
}

static void
gbp_project_tree_pane_class_init (GbpProjectTreePaneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_project_tree_pane_dispose;

  widget_class->grab_focus = gbp_project_tree_pane_grab_focus;
  widget_class->root = gbp_project_tree_pane_root;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/project-tree/gbp-project-tree-pane.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpProjectTreePane, list);
  gtk_widget_class_bind_template_child (widget_class, GbpProjectTreePane, search);
  gtk_widget_class_bind_template_child (widget_class, GbpProjectTreePane, selection);
  gtk_widget_class_bind_template_child (widget_class, GbpProjectTreePane, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpProjectTreePane, tree);
  gtk_widget_class_bind_template_callback (widget_class, on_search_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_list_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_search_activate_cb);

  g_type_ensure (GBP_TYPE_PROJECT_TREE);
}

static void
gbp_project_tree_pane_init (GbpProjectTreePane *self)
{
  IdeApplication *app;
  GMenu *menu;

  gtk_widget_init_template (GTK_WIDGET (self));

  app = IDE_APPLICATION_DEFAULT;
  menu = ide_application_get_menu_by_id (IDE_APPLICATION (app), "project-tree-menu");
  ide_tree_set_menu_model (self->tree, G_MENU_MODEL (menu));

  g_signal_connect_object (self->tree,
                           "notify::selected-node",
                           G_CALLBACK (_gbp_project_tree_pane_update_actions),
                           self,
                           G_CONNECT_SWAPPED);

  _gbp_project_tree_pane_init_actions (self);
}

GbpProjectTree *
gbp_project_tree_pane_get_tree (GbpProjectTreePane *self)
{
  g_return_val_if_fail (GBP_IS_PROJECT_TREE_PANE (self), NULL);

  return GBP_PROJECT_TREE (self->tree);
}
