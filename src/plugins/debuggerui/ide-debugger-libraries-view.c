/* ide-debugger-libraries-view.c
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

#define G_LOG_DOMAIN "ide-debugger-libraries-view"

#include "config.h"

#include <libide-gtk.h>

#include "ide-debugger-libraries-view.h"

struct _IdeDebuggerLibrariesView
{
  AdwBin parent_instance;

  /* Template widgets */
  GtkTreeView         *tree_view;
  GtkListStore        *list_store;
  GtkCellRendererText *range_cell;
  GtkTreeViewColumn   *range_column;
  GtkCellRendererText *target_cell;
  GtkTreeViewColumn   *target_column;

  /* Onwed refnerences */
  GSignalGroup   *debugger_signals;
};

enum {
  PROP_0,
  PROP_DEBUGGER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeDebuggerLibrariesView, ide_debugger_libraries_view, ADW_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_libraries_view_bind (IdeDebuggerLibrariesView *self,
                                  IdeDebugger              *debugger,
                                  GSignalGroup             *signals)
{
  g_assert (IDE_IS_DEBUGGER_LIBRARIES_VIEW (self));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view),
                            !ide_debugger_get_is_running (debugger));
}

static void
ide_debugger_libraries_view_unbind (IdeDebuggerLibrariesView *self,
                                    GSignalGroup             *signals)
{
  g_assert (IDE_IS_DEBUGGER_LIBRARIES_VIEW (self));
  g_assert (G_IS_SIGNAL_GROUP (signals));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view), FALSE);
}

static void
ide_debugger_libraries_view_running (IdeDebuggerLibrariesView *self,
                                     IdeDebugger              *debugger)
{
  g_assert (IDE_IS_DEBUGGER_LIBRARIES_VIEW (self));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view), FALSE);
}

static void
ide_debugger_libraries_view_stopped (IdeDebuggerLibrariesView *self,
                                     IdeDebuggerStopReason     stop_reason,
                                     IdeDebuggerBreakpoint    *breakpoint,
                                     IdeDebugger              *debugger)
{
  g_assert (IDE_IS_DEBUGGER_LIBRARIES_VIEW (self));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view), TRUE);
}

static void
ide_debugger_libraries_view_library_loaded (IdeDebuggerLibrariesView *self,
                                            IdeDebuggerLibrary       *library,
                                            IdeDebugger              *debugger)
{
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_LIBRARIES_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_LIBRARY (library));
  g_assert (IDE_IS_DEBUGGER (debugger));

  ide_gtk_list_store_insert_sorted (self->list_store,
                                    &iter, library, 0,
                                    (GCompareDataFunc)ide_debugger_library_compare,
                                    NULL);

  gtk_list_store_set (self->list_store, &iter, 0, library, -1);
}

static void
ide_debugger_libraries_view_library_unloaded (IdeDebuggerLibrariesView *self,
                                              IdeDebuggerLibrary       *library,
                                              IdeDebugger              *debugger)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_LIBRARIES_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_LIBRARY (library));
  g_assert (IDE_IS_DEBUGGER (debugger));

  model = GTK_TREE_MODEL (self->list_store);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          g_autoptr(IdeDebuggerLibrary) element = NULL;

          gtk_tree_model_get (model, &iter, 0, &element, -1);

          if (ide_debugger_library_compare (library, element) == 0)
            {
              gtk_list_store_remove (self->list_store, &iter);
              break;
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

static void
range_cell_data_func (GtkCellLayout   *cell_layout,
                      GtkCellRenderer *cell,
                      GtkTreeModel    *model,
                      GtkTreeIter     *iter,
                      gpointer         user_data)
{
  g_autoptr(IdeDebuggerLibrary) library = NULL;
  g_autofree gchar *str = NULL;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  gtk_tree_model_get (model, iter, 0, &library, -1);

  if (library != NULL)
    {
      GPtrArray *ranges = ide_debugger_library_get_ranges (library);

      if (ranges != NULL && ranges->len > 0)
        {
          IdeDebuggerAddressRange *range = g_ptr_array_index (ranges, 0);

          str = g_strdup_printf ("0x%"G_GINT64_MODIFIER"x - 0x%"G_GINT64_MODIFIER"x",
                                 range->from, range->to);
        }
    }

  g_object_set (cell, "text", str, NULL);
}

static void
string_property_cell_data_func (GtkCellLayout   *cell_layout,
                                GtkCellRenderer *cell,
                                GtkTreeModel    *model,
                                GtkTreeIter     *iter,
                                gpointer         user_data)
{
  const gchar *property = user_data;
  g_autoptr(GObject) object = NULL;
  g_auto(GValue) value = G_VALUE_INIT;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (property != NULL);

  g_value_init (&value, G_TYPE_STRING);
  gtk_tree_model_get (model, iter, 0, &object, -1);

  if (object != NULL)
    g_object_get_property (object, property, &value);

  g_object_set_property (G_OBJECT (cell), "text", &value);
}

static void
ide_debugger_libraries_view_dispose (GObject *object)
{
  IdeDebuggerLibrariesView *self = (IdeDebuggerLibrariesView *)object;

  g_clear_object (&self->debugger_signals);

  G_OBJECT_CLASS (ide_debugger_libraries_view_parent_class)->dispose (object);
}

static void
ide_debugger_libraries_view_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeDebuggerLibrariesView *self = IDE_DEBUGGER_LIBRARIES_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      g_value_set_object (value, ide_debugger_libraries_view_get_debugger (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_libraries_view_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeDebuggerLibrariesView *self = IDE_DEBUGGER_LIBRARIES_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      ide_debugger_libraries_view_set_debugger (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_libraries_view_class_init (IdeDebuggerLibrariesViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_debugger_libraries_view_dispose;
  object_class->get_property = ide_debugger_libraries_view_get_property;
  object_class->set_property = ide_debugger_libraries_view_set_property;

  properties [PROP_DEBUGGER] =
    g_param_spec_object ("debugger",
                         "Debugger",
                         "The debugger instance",
                         IDE_TYPE_DEBUGGER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/debuggerui/ide-debugger-libraries-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLibrariesView, tree_view);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLibrariesView, list_store);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLibrariesView, target_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLibrariesView, target_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLibrariesView, range_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLibrariesView, range_column);

  g_type_ensure (IDE_TYPE_DEBUGGER_LIBRARY);
}

static void
ide_debugger_libraries_view_init (IdeDebuggerLibrariesView *self)
{
  g_autoptr(PangoAttrList) tt_attrs = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  tt_attrs = pango_attr_list_new ();
  pango_attr_list_insert (tt_attrs, pango_attr_family_new ("Monospace"));
  pango_attr_list_insert (tt_attrs, pango_attr_scale_new (0.83333));
  g_object_set (self->range_cell,
                "attributes", tt_attrs,
                NULL);

  self->debugger_signals = g_signal_group_new (IDE_TYPE_DEBUGGER);

  g_signal_connect_swapped (self->debugger_signals,
                            "bind",
                            G_CALLBACK (ide_debugger_libraries_view_bind),
                            self);

  g_signal_connect_swapped (self->debugger_signals,
                            "unbind",
                            G_CALLBACK (ide_debugger_libraries_view_unbind),
                            self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "running",
                                    G_CALLBACK (ide_debugger_libraries_view_running),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "stopped",
                                    G_CALLBACK (ide_debugger_libraries_view_stopped),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "library-loaded",
                                    G_CALLBACK (ide_debugger_libraries_view_library_loaded),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "library-unloaded",
                                    G_CALLBACK (ide_debugger_libraries_view_library_unloaded),
                                    self);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->target_column),
                                      GTK_CELL_RENDERER (self->target_cell),
                                      string_property_cell_data_func, (gchar *)"target-name", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->range_column),
                                      GTK_CELL_RENDERER (self->range_cell),
                                      range_cell_data_func, NULL, NULL);
}

GtkWidget *
ide_debugger_libraries_view_new (void)
{
  return g_object_new (IDE_TYPE_DEBUGGER_LIBRARIES_VIEW, NULL);
}

/**
 * ide_debugger_libraries_view_get_debugger:
 * @self: a #IdeDebuggerLibrariesView
 *
 * Gets the debugger property.
 *
 * Returns: (transfer none): An #IdeDebugger or %NULL.
 */
IdeDebugger *
ide_debugger_libraries_view_get_debugger (IdeDebuggerLibrariesView *self)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_LIBRARIES_VIEW (self), NULL);

  if (self->debugger_signals != NULL)
    return _g_signal_group_get_target (self->debugger_signals);
  return NULL;
}

void
ide_debugger_libraries_view_set_debugger (IdeDebuggerLibrariesView *self,
                                          IdeDebugger              *debugger)
{
  g_return_if_fail (IDE_IS_DEBUGGER_LIBRARIES_VIEW (self));
  g_return_if_fail (!debugger || IDE_IS_DEBUGGER (debugger));

  g_signal_group_set_target (self->debugger_signals, debugger);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUGGER]);
}
