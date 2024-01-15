/* ide-debugger-breakpoints-view.c
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

#define G_LOG_DOMAIN "ide-debugger-breakpoints-view"

#include "config.h"

#include <libide-gtk.h>

#include "ide-debugger-breakpoints-view.h"

struct _IdeDebuggerBreakpointsView
{
  AdwBin                 parent_instance;

  /* Owned references */
  GSignalGroup          *debugger_signals;

  /* Template references */
  GtkCellRendererText   *address_cell;
  GtkCellRendererText   *file_cell;
  GtkCellRendererText   *function_cell;
  GtkCellRendererText   *hits_cell;
  GtkCellRendererText   *id_cell;
  GtkCellRendererText   *line_cell;
  GtkCellRendererText   *spec_cell;
  GtkCellRendererText   *type_cell;
  GtkCellRendererToggle *enabled_cell;
  GtkListStore          *list_store;
  GtkTreeView           *tree_view;
  GtkTreeViewColumn     *address_column;
  GtkTreeViewColumn     *enabled_column;
  GtkTreeViewColumn     *file_column;
  GtkTreeViewColumn     *function_column;
  GtkTreeViewColumn     *hits_column;
  GtkTreeViewColumn     *id_column;
  GtkTreeViewColumn     *line_column;
  GtkTreeViewColumn     *spec_column;
  GtkTreeViewColumn     *type_column;
};

enum {
  PROP_0,
  PROP_DEBUGGER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeDebuggerBreakpointsView, ide_debugger_breakpoints_view, ADW_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_breakpoints_view_bind (IdeDebuggerBreakpointsView *self,
                                    IdeDebugger                *debugger,
                                    GSignalGroup               *debugger_signals)
{
  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (G_IS_SIGNAL_GROUP (debugger_signals));

  gtk_list_store_clear (self->list_store);
}

static void
ide_debugger_breakpoints_view_running (IdeDebuggerBreakpointsView *self,
                                       IdeDebugger                *debugger)
{
  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view), FALSE);
}

static void
ide_debugger_breakpoints_view_stopped (IdeDebuggerBreakpointsView *self,
                                       IdeDebuggerStopReason       stop_reason,
                                       IdeDebuggerBreakpoint      *breakpoint,
                                       IdeDebugger                *debugger)
{
  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_STOP_REASON (stop_reason));
  g_assert (!breakpoint || IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view), TRUE);
}

static void
ide_debugger_breakpoints_view_breakpoint_added (IdeDebuggerBreakpointsView *self,
                                                IdeDebuggerBreakpoint      *breakpoint,
                                                IdeDebugger                *debugger)
{
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  ide_gtk_list_store_insert_sorted (self->list_store, &iter, breakpoint, 0,
                                    (GCompareDataFunc)ide_debugger_breakpoint_compare,
                                    NULL);

  gtk_list_store_set (self->list_store, &iter, 0, breakpoint, -1);
}

static void
ide_debugger_breakpoints_view_breakpoint_removed (IdeDebuggerBreakpointsView *self,
                                                  IdeDebuggerBreakpoint      *breakpoint,
                                                  IdeDebugger                *debugger)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  model = GTK_TREE_MODEL (self->list_store);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          g_autoptr(IdeDebuggerBreakpoint) row = NULL;

          gtk_tree_model_get (model, &iter, 0, &row, -1);

          if (ide_debugger_breakpoint_compare (row, breakpoint) == 0)
            {
              gtk_list_store_remove (self->list_store, &iter);
              break;
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

static void
ide_debugger_breakpoints_view_breakpoint_modified (IdeDebuggerBreakpointsView *self,
                                                   IdeDebuggerBreakpoint      *breakpoint,
                                                   IdeDebugger                *debugger)
{
  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  /* We can optimize this into a single replace, but should be fine for now */
  ide_debugger_breakpoints_view_breakpoint_removed (self, breakpoint, debugger);
  ide_debugger_breakpoints_view_breakpoint_added (self, breakpoint, debugger);
}

static void
ide_debugger_breakpoints_view_enabled_toggled (IdeDebuggerBreakpointsView *self,
                                               const gchar                *path_str,
                                               GtkCellRendererToggle      *cell)
{
  IdeDebugger *debugger;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS_VIEW (self));
  g_assert (path_str != NULL);
  g_assert (GTK_IS_CELL_RENDERER_TOGGLE (cell));

  debugger = ide_debugger_breakpoints_view_get_debugger (self);
  if (debugger == NULL)
    return;

  model = GTK_TREE_MODEL (self->list_store);
  path = gtk_tree_path_new_from_string (path_str);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      g_autoptr(IdeDebuggerBreakpoint) breakpoint = NULL;

      gtk_tree_model_get (model, &iter, 0, &breakpoint, -1);

      ide_debugger_breakpoint_set_enabled (breakpoint,
                                           !ide_debugger_breakpoint_get_enabled (breakpoint));
      ide_debugger_modify_breakpoint_async (debugger,
                                            IDE_DEBUGGER_BREAKPOINT_CHANGE_ENABLED,
                                            breakpoint,
                                            NULL, NULL, NULL);
    }

  gtk_tree_path_free (path);
}

static void
address_cell_data_func (GtkCellLayout   *cell_layout,
                        GtkCellRenderer *cell,
                        GtkTreeModel    *model,
                        GtkTreeIter     *iter,
                        gpointer         user_data)
{
  g_autoptr(IdeDebuggerBreakpoint) breakpoint = NULL;
  IdeDebuggerAddress addr = IDE_DEBUGGER_ADDRESS_INVALID;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  gtk_tree_model_get (model, iter, 0, &breakpoint, -1);

  if (breakpoint != NULL)
    addr = ide_debugger_breakpoint_get_address (breakpoint);

  if (addr == IDE_DEBUGGER_ADDRESS_INVALID)
    {
      g_object_set (cell, "text", NULL, NULL);
    }
  else
    {
      g_autofree gchar *str = NULL;

      str = g_strdup_printf ("0x%"G_GINT64_MODIFIER"x", addr);
      g_object_set (cell, "text", str, NULL);
    }
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
int_property_cell_data_func (GtkCellLayout   *cell_layout,
                             GtkCellRenderer *cell,
                             GtkTreeModel    *model,
                             GtkTreeIter     *iter,
                             gpointer         user_data)
{
  const gchar *property = user_data;
  g_autoptr(GObject) object = NULL;
  g_auto(GValue) value = G_VALUE_INIT;
  g_autofree gchar *str = NULL;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (property != NULL);

  g_value_init (&value, G_TYPE_INT64);
  gtk_tree_model_get (model, iter, 0, &object, -1);

  if (object != NULL)
    g_object_get_property (object, property, &value);

  str = g_strdup_printf ("%"G_GINT64_FORMAT, g_value_get_int64 (&value));
  g_object_set (cell, "text", str, NULL);
}

static void
enum_property_cell_data_func (GtkCellLayout   *cell_layout,
                              GtkCellRenderer *cell,
                              GtkTreeModel    *model,
                              GtkTreeIter     *iter,
                              gpointer         user_data)
{
  GParamSpec *pspec = user_data;
  g_autoptr(GObject) object = NULL;
  g_auto(GValue) value = G_VALUE_INIT;
  const gchar *str = NULL;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (pspec != NULL);

  g_value_init (&value, pspec->value_type);
  gtk_tree_model_get (model, iter, 0, &object, -1);

  if (object != NULL)
    {
      GEnumValue *ev = g_enum_get_value (g_type_class_peek (pspec->value_type),
                                         g_value_get_enum (&value));

      if (ev != NULL)
        str = ev->value_nick;
    }

  g_object_set (cell, "text", str, NULL);
}

static void
bool_property_cell_data_func (GtkCellLayout   *cell_layout,
                              GtkCellRenderer *cell,
                              GtkTreeModel    *model,
                              GtkTreeIter     *iter,
                              gpointer         user_data)
{
  g_autoptr(GObject) object = NULL;
  g_auto(GValue) value = G_VALUE_INIT;
  const gchar *property = user_data;

  g_value_init (&value, G_TYPE_BOOLEAN);
  gtk_tree_model_get (model, iter, 0, &object, -1);
  if (object != NULL)
    g_object_get_property (object, property, &value);
  g_object_set_property (G_OBJECT (cell), "active", &value);
}

#if 0
static void
ide_debugger_breakpoints_view_delete_breakpoint (GtkTreeView                *tree_view,
                                                 IdeDebuggerBreakpointsView *self)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model = NULL;
  IdeDebugger *debugger;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS_VIEW (self));
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  debugger = ide_debugger_breakpoints_view_get_debugger (self);

  if (debugger == NULL)
    return;

  selection = gtk_tree_view_get_selection (tree_view);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      g_autoptr(IdeDebuggerBreakpoint) breakpoint = NULL;

      gtk_tree_model_get (model, &iter, 0, &breakpoint, -1);

      if (breakpoint != NULL)
        ide_debugger_remove_breakpoint_async (debugger, breakpoint, NULL, NULL, NULL);
    }
}
#endif

static void
ide_debugger_breakpoints_view_dispose (GObject *object)
{
  IdeDebuggerBreakpointsView *self = (IdeDebuggerBreakpointsView *)object;

  g_clear_object (&self->debugger_signals);

  G_OBJECT_CLASS (ide_debugger_breakpoints_view_parent_class)->dispose (object);
}

static void
ide_debugger_breakpoints_view_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  IdeDebuggerBreakpointsView *self = IDE_DEBUGGER_BREAKPOINTS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      g_value_set_object (value, ide_debugger_breakpoints_view_get_debugger (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_breakpoints_view_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  IdeDebuggerBreakpointsView *self = IDE_DEBUGGER_BREAKPOINTS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      ide_debugger_breakpoints_view_set_debugger (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_breakpoints_view_class_init (IdeDebuggerBreakpointsViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_debugger_breakpoints_view_dispose;
  object_class->get_property = ide_debugger_breakpoints_view_get_property;
  object_class->set_property = ide_debugger_breakpoints_view_set_property;

  properties [PROP_DEBUGGER] =
    g_param_spec_object ("debugger",
                         "Debugger",
                         "The debugger being observed",
                         IDE_TYPE_DEBUGGER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/debuggerui/ide-debugger-breakpoints-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, address_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, address_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, hits_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, hits_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, file_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, file_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, function_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, function_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, id_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, id_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, line_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, line_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, list_store);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, tree_view);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, spec_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, spec_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, type_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, type_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, enabled_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerBreakpointsView, enabled_column);

  g_type_ensure (IDE_TYPE_DEBUGGER_BREAKPOINT);
}

static void
ide_debugger_breakpoints_view_init (IdeDebuggerBreakpointsView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->debugger_signals = g_signal_group_new (IDE_TYPE_DEBUGGER);

  g_signal_connect_swapped (self->debugger_signals,
                            "bind",
                            G_CALLBACK (ide_debugger_breakpoints_view_bind),
                            self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "running",
                                    G_CALLBACK (ide_debugger_breakpoints_view_running),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "stopped",
                                    G_CALLBACK (ide_debugger_breakpoints_view_stopped),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "breakpoint-added",
                                    G_CALLBACK (ide_debugger_breakpoints_view_breakpoint_added),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "breakpoint-removed",
                                    G_CALLBACK (ide_debugger_breakpoints_view_breakpoint_removed),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "breakpoint-modified",
                                    G_CALLBACK (ide_debugger_breakpoints_view_breakpoint_modified),
                                    self);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->id_column),
                                      GTK_CELL_RENDERER (self->id_cell),
                                      string_property_cell_data_func, (gchar *)"id", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->file_column),
                                      GTK_CELL_RENDERER (self->file_cell),
                                      string_property_cell_data_func, (gchar *)"file", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->line_column),
                                      GTK_CELL_RENDERER (self->line_cell),
                                      int_property_cell_data_func, (gchar *)"line", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->function_column),
                                      GTK_CELL_RENDERER (self->function_cell),
                                      string_property_cell_data_func, (gchar *)"function", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->address_column),
                                      GTK_CELL_RENDERER (self->address_cell),
                                      address_cell_data_func, NULL, NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->hits_column),
                                      GTK_CELL_RENDERER (self->hits_cell),
                                      int_property_cell_data_func, (gchar *)"count", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->type_column),
                                      GTK_CELL_RENDERER (self->type_cell),
                                      enum_property_cell_data_func,
                                      g_object_class_find_property (g_type_class_peek (IDE_TYPE_DEBUGGER_BREAKPOINT), "mode"),
                                      NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->spec_column),
                                      GTK_CELL_RENDERER (self->spec_cell),
                                      string_property_cell_data_func, (gchar *)"spec", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->enabled_column),
                                      GTK_CELL_RENDERER (self->enabled_cell),
                                      bool_property_cell_data_func, (gchar *)"enabled", NULL);

  g_signal_connect_swapped (self->enabled_cell,
                            "toggled",
                            G_CALLBACK (ide_debugger_breakpoints_view_enabled_toggled),
                            self);
}

GtkWidget *
ide_debugger_breakpoints_view_new (void)
{
  return g_object_new (IDE_TYPE_DEBUGGER_BREAKPOINTS_VIEW, NULL);
}

/**
 * ide_debugger_breakpoints_view_get_debugger:
 * @self: a #IdeDebuggerBreakpointsView
 *
 * Gets the debugger that is being observed by the view.
 *
 * Returns: (nullable) (transfer none): An #IdeDebugger or %NULL
 */
IdeDebugger *
ide_debugger_breakpoints_view_get_debugger (IdeDebuggerBreakpointsView *self)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINTS_VIEW (self), NULL);

  if (self->debugger_signals != NULL)
    return _g_signal_group_get_target (self->debugger_signals);
  else
    return NULL;
}

/**
 * ide_debugger_breakpoints_view_set_debugger:
 * @self: a #IdeDebuggerBreakpointsView
 * @debugger: (nullable): An #IdeDebugger or %NULL
 *
 * Sets the debugger that is being viewed.
 */
void
ide_debugger_breakpoints_view_set_debugger (IdeDebuggerBreakpointsView *self,
                                            IdeDebugger                *debugger)
{
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINTS_VIEW (self));
  g_return_if_fail (!debugger || IDE_IS_DEBUGGER (debugger));

  if (self->debugger_signals != NULL)
    {
      g_signal_group_set_target (self->debugger_signals, debugger);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUGGER]);
    }
}
