/* ide-debugger-registers-view.c
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

#define G_LOG_DOMAIN "ide-debugger-registers-view"

#include "config.h"

#include <libide-core.h>
#include <libide-gtk.h>

#include "ide-debugger-registers-view.h"

struct _IdeDebuggerRegistersView
{
  AdwBin          parent_instance;

  /* Owned references */
  GSignalGroup        *debugger_signals;

  /* Template references */
  GtkTreeView         *tree_view;
  GtkListStore        *list_store;
  GtkCellRendererText *id_cell;
  GtkCellRendererText *name_cell;
  GtkCellRendererText *value_cell;
  GtkTreeViewColumn   *id_column;
  GtkTreeViewColumn   *name_column;
  GtkTreeViewColumn   *value_column;
};

enum {
  PROP_0,
  PROP_DEBUGGER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeDebuggerRegistersView, ide_debugger_registers_view, ADW_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_registers_view_bind (IdeDebuggerRegistersView *self,
                                  IdeDebugger              *debugger,
                                  GSignalGroup             *signals)
{
  g_assert (IDE_IS_DEBUGGER_REGISTERS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (G_IS_SIGNAL_GROUP (signals));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view),
                            !ide_debugger_get_is_running (debugger));
}

static void
ide_debugger_registers_view_unbind (IdeDebuggerRegistersView *self,
                                    GSignalGroup             *signals)
{
  g_assert (IDE_IS_DEBUGGER_REGISTERS_VIEW (self));
  g_assert (G_IS_SIGNAL_GROUP (signals));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view), FALSE);
}

static void
ide_debugger_registers_view_running (IdeDebuggerRegistersView *self,
                                     IdeDebugger              *debugger)
{
  g_assert (IDE_IS_DEBUGGER_REGISTERS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view), FALSE);
}

static void
ide_debugger_registers_view_list_registers_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  IdeDebugger *debugger = (IdeDebugger *)object;
  g_autoptr(IdeDebuggerRegistersView) self = user_data;
  g_autoptr(GPtrArray) registers = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DEBUGGER_REGISTERS_VIEW (self));

  gtk_list_store_clear (self->list_store);

  registers = ide_debugger_list_registers_finish (debugger, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (registers, g_object_unref);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
      return;
    }

  if (registers != NULL)
    {
      for (guint i = 0; i < registers->len; i++)
        {
          IdeDebuggerRegister *reg = g_ptr_array_index (registers, i);
          GtkTreeIter iter;

          ide_gtk_list_store_insert_sorted (self->list_store, &iter, reg, 0,
                                            (GCompareDataFunc)ide_debugger_register_compare,
                                            NULL);
          gtk_list_store_set (self->list_store, &iter, 0, reg, -1);
        }
    }
}

static void
ide_debugger_registers_view_stopped (IdeDebuggerRegistersView *self,
                                     IdeDebuggerStopReason     stop_reason,
                                     IdeDebuggerBreakpoint    *breakpoint,
                                     IdeDebugger              *debugger)
{
  g_assert (IDE_IS_DEBUGGER_REGISTERS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_STOP_REASON (stop_reason));
  g_assert (IDE_IS_DEBUGGER (debugger));

  ide_debugger_list_registers_async (debugger,
                                     NULL,
                                     ide_debugger_registers_view_list_registers_cb,
                                     g_object_ref (self));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view), TRUE);
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
ide_debugger_registers_view_dispose (GObject *object)
{
  IdeDebuggerRegistersView *self = (IdeDebuggerRegistersView *)object;

  g_clear_object (&self->debugger_signals);

  G_OBJECT_CLASS (ide_debugger_registers_view_parent_class)->dispose (object);
}

static void
ide_debugger_registers_view_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeDebuggerRegistersView *self = IDE_DEBUGGER_REGISTERS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      g_value_set_object (value, ide_debugger_registers_view_get_debugger (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_registers_view_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeDebuggerRegistersView *self = IDE_DEBUGGER_REGISTERS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      ide_debugger_registers_view_set_debugger (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_registers_view_class_init (IdeDebuggerRegistersViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_debugger_registers_view_dispose;
  object_class->get_property = ide_debugger_registers_view_get_property;
  object_class->set_property = ide_debugger_registers_view_set_property;

  properties [PROP_DEBUGGER] =
    g_param_spec_object ("debugger",
                         "Debugger",
                         "The debugger instance",
                         IDE_TYPE_DEBUGGER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/debuggerui/ide-debugger-registers-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerRegistersView, id_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerRegistersView, id_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerRegistersView, list_store);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerRegistersView, name_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerRegistersView, name_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerRegistersView, tree_view);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerRegistersView, value_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerRegistersView, value_column);

  g_type_ensure (IDE_TYPE_DEBUGGER_REGISTER);
}

static void
ide_debugger_registers_view_init (IdeDebuggerRegistersView *self)
{
  self->debugger_signals = g_signal_group_new (IDE_TYPE_DEBUGGER);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_swapped (self->debugger_signals,
                            "bind",
                            G_CALLBACK (ide_debugger_registers_view_bind),
                            self);

  g_signal_connect_swapped (self->debugger_signals,
                            "unbind",
                            G_CALLBACK (ide_debugger_registers_view_unbind),
                            self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "running",
                                    G_CALLBACK (ide_debugger_registers_view_running),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "stopped",
                                    G_CALLBACK (ide_debugger_registers_view_stopped),
                                    self);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->id_column),
                                      GTK_CELL_RENDERER (self->id_cell),
                                      string_property_cell_data_func, (gchar *)"id", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->name_column),
                                      GTK_CELL_RENDERER (self->name_cell),
                                      string_property_cell_data_func, (gchar *)"name", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->value_column),
                                      GTK_CELL_RENDERER (self->value_cell),
                                      string_property_cell_data_func, (gchar *)"value", NULL);
}

GtkWidget *
ide_debugger_registers_view_new (void)
{
  return g_object_new (IDE_TYPE_DEBUGGER_REGISTERS_VIEW, NULL);
}

/**
 * ide_debugger_registers_view_get_debugger:
 * @self: a #IdeDebuggerRegistersView
 *
 *
 *
 * Returns: (transfer none) (nullable): An #IdeDebugger or %NULL
 */
IdeDebugger *
ide_debugger_registers_view_get_debugger (IdeDebuggerRegistersView *self)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_REGISTERS_VIEW (self), NULL);

  if (self->debugger_signals != NULL)
    return _g_signal_group_get_target (self->debugger_signals);

  return NULL;
}

void
ide_debugger_registers_view_set_debugger (IdeDebuggerRegistersView *self,
                                          IdeDebugger              *debugger)
{
  g_return_if_fail (IDE_IS_DEBUGGER_REGISTERS_VIEW (self));
  g_return_if_fail (!debugger || IDE_IS_DEBUGGER (debugger));

  if (self->debugger_signals != NULL)
    {
      g_signal_group_set_target (self->debugger_signals, debugger);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUGGER]);
    }
}
