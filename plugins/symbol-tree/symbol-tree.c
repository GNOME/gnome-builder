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

#include <glib/gi18n.h>
#include <libpeas/peas.h>


#include "gb-plugins.h"
#include "gb-tree.h"
#include "gb-workspace.h"

#include "symbol-tree.h"
#include "symbol-tree-resources.h"

struct _SymbolTree
{
  GtkBox       parent_instance;

  GbWorkbench *workbench;

  GbTree      *tree;
};

enum {
  PROP_0,
  PROP_WORKBENCH,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void workbench_addin_init (GbWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (SymbolTree, symbol_tree, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GB_TYPE_WORKBENCH_ADDIN, workbench_addin_init))

static void
symbol_tree_load (GbWorkbenchAddin *addin)
{
  SymbolTree *self = (SymbolTree *)addin;
  GbWorkspace *workspace;
  GtkWidget *right_pane;

  g_assert (SYMBOL_IS_TREE (self));
  g_assert (GB_IS_WORKBENCH (self->workbench));

  workspace = GB_WORKSPACE (gb_workbench_get_workspace (self->workbench));
  right_pane = gb_workspace_get_right_pane (workspace);
  gb_workspace_pane_add_page (GB_WORKSPACE_PANE (right_pane),
                              GTK_WIDGET (self),
                              _("Symbol Tree"),
                              "lang-function-symbolic");
}

static void
symbol_tree_unload (GbWorkbenchAddin *addin)
{
  SymbolTree *self = (SymbolTree *)addin;
  GbWorkspace *workspace;
  GtkWidget *right_pane;

  g_assert (SYMBOL_IS_TREE (self));
  g_assert (GB_IS_WORKBENCH (self->workbench));

  workspace = GB_WORKSPACE (gb_workbench_get_workspace (self->workbench));
  right_pane = gb_workspace_get_right_pane (workspace);
  gb_workspace_pane_remove_page (GB_WORKSPACE_PANE (right_pane), GTK_WIDGET (self));
}

static void
symbol_tree_set_workbench (SymbolTree  *self,
                           GbWorkbench *workbench)
{
  g_assert (SYMBOL_IS_TREE (self));
  g_assert (GB_IS_WORKBENCH (workbench));

  ide_set_weak_pointer (&self->workbench, workbench);
}

static void
symbol_tree_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  SymbolTree *self = (SymbolTree *)object;

  switch (prop_id)
    {
    case PROP_WORKBENCH:
      symbol_tree_set_workbench (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
symbol_tree_finalize (GObject *object)
{
  SymbolTree *self = (SymbolTree *)object;

  ide_clear_weak_pointer (&self->workbench);

  G_OBJECT_CLASS (symbol_tree_parent_class)->finalize (object);
}

static void
workbench_addin_init (GbWorkbenchAddinInterface *iface)
{
  iface->load = symbol_tree_load;
  iface->unload = symbol_tree_unload;
}

static void
symbol_tree_class_init (SymbolTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = symbol_tree_finalize;
  object_class->set_property = symbol_tree_set_property;

  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("Workbench"),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/symbol-tree/symbol-tree.ui");
  gtk_widget_class_bind_template_child (widget_class, SymbolTree, tree);

  g_type_ensure (GB_TYPE_TREE);
}

static void
symbol_tree_init (SymbolTree *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GB_DEFINE_EMBEDDED_PLUGIN (symbol_tree,
                           symbol_tree_get_resource (),
                           "resource:///org/gnome/builder/plugins/symbol-tree/symbol-tree.plugin",
                           GB_DEFINE_PLUGIN_TYPE (GB_TYPE_WORKBENCH_ADDIN, SYMBOL_TYPE_TREE))
