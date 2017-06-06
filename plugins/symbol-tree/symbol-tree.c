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
  IdeBuffer       *buffer;
};

static void workbench_addin_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (SymbolTree, symbol_tree, IDE_TYPE_OBJECT, 0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (IDE_TYPE_WORKBENCH_ADDIN,
                                                               workbench_addin_init))

static void
symbol_tree_symbol_resolver_loaded_cb (SymbolTree *self,
                                       IdeBuffer  *buffer)
{
  g_assert (SYMBOL_IS_TREE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  symbol_tree_panel_reset (self->panel);
}

static void
notify_active_view_cb (SymbolTree  *self,
                       GParamFlags *pspec,
                       IdeLayout   *layout)
{
  GtkWidget *active_view;
  IdeBuffer *buffer;

  IDE_ENTRY;

  g_assert (SYMBOL_IS_TREE (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_LAYOUT (layout));

  symbol_tree_panel_reset (self->panel);

  if (self->buffer != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->buffer,
                                            symbol_tree_symbol_resolver_loaded_cb,
                                            self);
      ide_clear_weak_pointer (&self->buffer);
    }

  active_view = ide_layout_get_active_view (layout);
  if (IDE_IS_EDITOR_VIEW (active_view))
    {
      buffer = ide_editor_view_get_document (IDE_EDITOR_VIEW (active_view));
      if (ide_buffer_get_symbol_resolver (buffer) == NULL)
        {
          ide_set_weak_pointer (&self->buffer, buffer);

          g_signal_connect_object (buffer,
                                   "symbol-resolver-loaded",
                                   G_CALLBACK (symbol_tree_symbol_resolver_loaded_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
        }
    }

  IDE_EXIT;
}

static void
symbol_tree_load (IdeWorkbenchAddin *addin,
                  IdeWorkbench      *workbench)
{
  SymbolTree *self = (SymbolTree *)addin;
  IdePerspective *perspective;
  IdeLayout *layout;
  GtkWidget *right_pane;

  g_assert (SYMBOL_IS_TREE (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  perspective = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (perspective != NULL);
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (perspective));

  layout = ide_editor_perspective_get_layout (IDE_EDITOR_PERSPECTIVE (perspective));
  g_signal_connect_object (layout,
                           "notify::active-view",
                           G_CALLBACK (notify_active_view_cb),
                           self,
                           G_CONNECT_SWAPPED);

  right_pane = ide_editor_perspective_get_right_edge (IDE_EDITOR_PERSPECTIVE (perspective));
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
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (perspective));

  pane = ide_editor_perspective_get_right_edge (IDE_EDITOR_PERSPECTIVE (perspective));
  g_assert (IDE_IS_LAYOUT_PANE (pane));

  if (self->buffer != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->buffer,
                                            symbol_tree_symbol_resolver_loaded_cb,
                                            self);
      ide_clear_weak_pointer (&self->buffer);
    }

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
  g_type_ensure (DZL_TYPE_TREE);
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
