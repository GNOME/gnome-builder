/* symbol-tree.c
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

#define G_LOG_DOMAIN "symbol-tree"

#include <glib/gi18n.h>
#include <ide.h>
#include <libpeas/peas.h>

#include "symbol-tree.h"
#include "symbol-tree-panel.h"

struct _SymbolTree
{
  GObject          parent_instance;
  SymbolTreePanel *panel;
};

static void workbench_addin_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (SymbolTree, symbol_tree, G_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (IDE_TYPE_WORKBENCH_ADDIN,
                                                               workbench_addin_init))

static void
notify_active_view_cb (SymbolTree  *self,
                       GParamFlags *pspec,
                       IdeLayout   *layout)
{
  IDE_ENTRY;

  g_assert (SYMBOL_IS_TREE (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_LAYOUT (layout));

  symbol_tree_panel_reset (self->panel);

  IDE_EXIT;
}

static void
symbol_tree_load (IdeWorkbenchAddin *addin,
                  IdeWorkbench      *workbench)
{
  SymbolTree *self = (SymbolTree *)addin;
  IdePerspective *perspective;
  GtkWidget *right_pane;

  g_assert (SYMBOL_IS_TREE (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  perspective = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (perspective != NULL);
  g_assert (IDE_IS_LAYOUT (perspective));

  g_signal_connect_object (perspective,
                           "notify::active-view",
                           G_CALLBACK (notify_active_view_cb),
                           self,
                           G_CONNECT_SWAPPED);

  right_pane = pnl_dock_bin_get_right_edge (PNL_DOCK_BIN (perspective));
  g_assert (right_pane != NULL);

  self->panel = g_object_new (SYMBOL_TYPE_TREE_PANEL,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (right_pane), GTK_WIDGET (self->panel));

  gtk_container_child_set (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (self->panel))),
                           GTK_WIDGET (self->panel),
                           "position", 1,
                           NULL);

  symbol_tree_panel_reset (self->panel);
}

static void
symbol_tree_unload (IdeWorkbenchAddin *addin,
                    IdeWorkbench      *workbench)
{
  SymbolTree *self = (SymbolTree *)addin;
  IdePerspective *perspective;
  GtkWidget *pane;

  g_assert (SYMBOL_IS_TREE (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  perspective = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (IDE_IS_LAYOUT (perspective));

  pane = pnl_dock_bin_get_right_edge (PNL_DOCK_BIN (perspective));
  g_assert (IDE_IS_LAYOUT_PANE (pane));

  gtk_widget_destroy (GTK_WIDGET (self->panel));
  self->panel = NULL;
}


static void
workbench_addin_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = symbol_tree_load;
  iface->unload = symbol_tree_unload;
}

static void
symbol_tree_class_init (SymbolTreeClass *klass)
{
  g_type_ensure (IDE_TYPE_TREE);
}

static void
symbol_tree_class_finalize (SymbolTreeClass *klass)
{
}

static void
symbol_tree_init (SymbolTree *self)
{
}

void
peas_register_types (PeasObjectModule *module)
{
  symbol_tree_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              SYMBOL_TYPE_TREE);
}
