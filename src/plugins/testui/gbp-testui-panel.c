/* gbp-testui-panel.c
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

#define G_LOG_DOMAIN "gbp-testui-panel"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-testui-item.h"
#include "gbp-testui-panel.h"

struct _GbpTestuiPanel
{
  IdePane           parent_instance;
  GtkNoSelection   *selection;
  GtkTreeListModel *tree_model;
};

G_DEFINE_FINAL_TYPE (GbpTestuiPanel, gbp_testui_panel, IDE_TYPE_PANE)

enum {
  TEST_ACTIVATED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
gbp_testui_panel_set_test_manager (GbpTestuiPanel *self,
                                   IdeTestManager *test_manager)
{
  g_autoptr(GbpTestuiItem) item = NULL;
  GListStore *store;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_PANEL (self));
  g_assert (IDE_IS_TEST_MANAGER (test_manager));
  g_assert (self->tree_model == NULL);

  store = g_list_store_new (GBP_TYPE_TESTUI_ITEM);
  item = gbp_testui_item_new (test_manager);
  g_list_store_append (store, item);

  self->tree_model = gtk_tree_list_model_new (G_LIST_MODEL (store),
                                              FALSE, TRUE,
                                              gbp_testui_item_create_child_model,
                                              NULL, NULL);
  gtk_no_selection_set_model (self->selection, G_LIST_MODEL (self->tree_model));


  IDE_EXIT;
}

static void
gbp_testui_panel_activate_cb (GbpTestuiPanel *self,
                              guint           position,
                              GtkListView    *list_view)
{
  g_autoptr(GtkTreeListRow) row = NULL;
  GtkSelectionModel *model;
  GbpTestuiItem *item;
  gpointer instance;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TESTUI_PANEL (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  model = gtk_list_view_get_model (list_view);

  row = g_list_model_get_item (G_LIST_MODEL (model), position);
  g_assert (GTK_IS_TREE_LIST_ROW (row));

  item = gtk_tree_list_row_get_item (row);
  g_assert (GBP_IS_TESTUI_ITEM (item));

  instance = gbp_testui_item_get_instance (item);
  g_assert (G_IS_OBJECT (instance));

  if (IDE_IS_TEST (instance))
    g_signal_emit (self, signals[TEST_ACTIVATED], 0, instance);

  IDE_EXIT;
}

static void
gbp_testui_panel_dispose (GObject *object)
{
  GbpTestuiPanel *self = (GbpTestuiPanel *)object;

  g_clear_object (&self->tree_model);

  G_OBJECT_CLASS (gbp_testui_panel_parent_class)->dispose (object);
}

static void
gbp_testui_panel_class_init (GbpTestuiPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_testui_panel_dispose;

  signals[TEST_ACTIVATED] =
    g_signal_new ("test-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_TEST);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/testui/gbp-testui-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpTestuiPanel, selection);
  gtk_widget_class_bind_template_callback (widget_class, gbp_testui_panel_activate_cb);
}

static void
gbp_testui_panel_init (GbpTestuiPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GbpTestuiPanel *
gbp_testui_panel_new (IdeTestManager *test_manager)
{
  GbpTestuiPanel *self;

  g_return_val_if_fail (IDE_IS_TEST_MANAGER (test_manager), NULL);

  self = g_object_new (GBP_TYPE_TESTUI_PANEL, NULL);
  gbp_testui_panel_set_test_manager (self, test_manager);

  return self;
}
