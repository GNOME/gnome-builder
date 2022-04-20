/* gbp-ls-tree-view.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-ls-tree-view"

#include "config.h"

#include "gbp-ls-model.h"
#include "gbp-ls-tree-view.h"

struct _GbpLsTreeView
{
  GtkTreeView  parent_instance;
};

enum {
  GO_UP,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

G_DEFINE_FINAL_TYPE (GbpLsTreeView, gbp_ls_tree_view, GTK_TYPE_TREE_VIEW)

static void
gbp_ls_tree_view_go_up (GbpLsTreeView *self)
{
  g_autoptr(GFile) parent = NULL;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GFile *directory;

  g_assert (GBP_IS_LS_TREE_VIEW (self));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self));

  if (!GBP_IS_LS_MODEL (model) ||
      !(directory = gbp_ls_model_get_directory (GBP_LS_MODEL (model))) ||
      !(parent = g_file_get_parent (directory)))
    return;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      g_autoptr(GtkTreePath) path = gtk_tree_model_get_path (model, &iter);
      GtkTreeViewColumn *column = gtk_tree_view_get_column (GTK_TREE_VIEW (self), 0);

      gtk_tree_selection_select_iter (selection, &iter);
      gtk_tree_view_row_activated (GTK_TREE_VIEW (self), path, column);
    }
}

static void
gbp_ls_tree_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (gbp_ls_tree_view_parent_class)->finalize (object);
}

static void
gbp_ls_tree_view_class_init (GbpLsTreeViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_ls_tree_view_finalize;

  signals [GO_UP] =
    g_signal_new_class_handler ("go-up",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gbp_ls_tree_view_go_up),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Up, GDK_ALT_MASK, "go-up", 0);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_BackSpace, 0, "go-up", 0);
}

static void
gbp_ls_tree_view_init (GbpLsTreeView *self)
{
}
