/* ide-debugger-locals-view.c
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

#define G_LOG_DOMAIN "ide-debugger-locals-view"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-core.h>
#include <libide-threading.h>

#include "ide-debugger-locals-view.h"

struct _IdeDebuggerLocalsView
{
  AdwBin          parent_instance;

  /* Owned references */
  GSignalGroup   *debugger_signals;

  /* Template references */
  GtkTreeStore        *tree_store;
  GtkTreeView         *tree_view;
  GtkTreeViewColumn   *type_column;
  GtkCellRendererText *type_cell;
  GtkTreeViewColumn   *variable_column;
  GtkCellRendererText *variable_cell;
  GtkTreeViewColumn   *value_column;
  GtkCellRendererText *value_cell;
};

enum {
  PROP_0,
  PROP_DEBUGGER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeDebuggerLocalsView, ide_debugger_locals_view, ADW_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_locals_view_running (IdeDebuggerLocalsView *self,
                                  IdeDebugger           *debugger)
{
  g_assert (IDE_IS_DEBUGGER_LOCALS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view), FALSE);
  gtk_tree_store_clear (self->tree_store);
}

static void
ide_debugger_locals_view_stopped (IdeDebuggerLocalsView *self,
                                  IdeDebuggerStopReason  stop_reason,
                                  IdeDebuggerBreakpoint *breakpoint,
                                  IdeDebugger           *debugger)
{
  g_assert (IDE_IS_DEBUGGER_LOCALS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_STOP_REASON (stop_reason));
  g_assert (!breakpoint || IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_widget_set_sensitive (GTK_WIDGET (self->tree_view), TRUE);
}

static void
name_cell_data_func (GtkCellLayout   *cell_layout,
                     GtkCellRenderer *cell,
                     GtkTreeModel    *model,
                     GtkTreeIter     *iter,
                     gpointer         user_data)
{
  g_autoptr(IdeDebuggerVariable) var = NULL;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  gtk_tree_model_get (model, iter, 0, &var, -1);

  if (var != NULL)
    {
      g_object_set (cell,
                    "text", ide_debugger_variable_get_name (var),
                    NULL);
    }
  else
    {
      g_autofree gchar *str = NULL;

      gtk_tree_model_get (model, iter, 1, &str, -1);
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
ide_debugger_locals_view_finalize (GObject *object)
{
  IdeDebuggerLocalsView *self = (IdeDebuggerLocalsView *)object;

  g_clear_object (&self->debugger_signals);

  G_OBJECT_CLASS (ide_debugger_locals_view_parent_class)->finalize (object);
}

static void
ide_debugger_locals_view_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeDebuggerLocalsView *self = IDE_DEBUGGER_LOCALS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      g_value_set_object (value, ide_debugger_locals_view_get_debugger (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_locals_view_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeDebuggerLocalsView *self = IDE_DEBUGGER_LOCALS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      ide_debugger_locals_view_set_debugger (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_locals_view_class_init (IdeDebuggerLocalsViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_debugger_locals_view_finalize;
  object_class->get_property = ide_debugger_locals_view_get_property;
  object_class->set_property = ide_debugger_locals_view_set_property;

  properties [PROP_DEBUGGER] =
    g_param_spec_object ("debugger",
                         "Debugger",
                         "The debugger instance",
                         IDE_TYPE_DEBUGGER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/debuggerui/ide-debugger-locals-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLocalsView, tree_store);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLocalsView, tree_view);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLocalsView, type_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLocalsView, type_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLocalsView, value_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLocalsView, value_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLocalsView, variable_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLocalsView, variable_column);
}

static void
ide_debugger_locals_view_init (IdeDebuggerLocalsView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->debugger_signals = g_signal_group_new (IDE_TYPE_DEBUGGER);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "running",
                                    G_CALLBACK (ide_debugger_locals_view_running),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "stopped",
                                    G_CALLBACK (ide_debugger_locals_view_stopped),
                                    self);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->variable_column),
                                      GTK_CELL_RENDERER (self->variable_cell),
                                      name_cell_data_func, NULL, NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->type_column),
                                      GTK_CELL_RENDERER (self->type_cell),
                                      string_property_cell_data_func, (gchar *)"type-name", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->value_column),
                                      GTK_CELL_RENDERER (self->value_cell),
                                      string_property_cell_data_func, (gchar *)"value", NULL);
}

GtkWidget *
ide_debugger_locals_view_new (void)
{
  return g_object_new (IDE_TYPE_DEBUGGER_LOCALS_VIEW, NULL);
}

/**
 * ide_debugger_locals_view_get_debugger:
 * @self: a #IdeDebuggerLocalsView
 *
 * Gets the debugger instance.
 *
 * Returns: (transfer none): An #IdeDebugger
 */
IdeDebugger *
ide_debugger_locals_view_get_debugger (IdeDebuggerLocalsView *self)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_LOCALS_VIEW (self), NULL);

  return _g_signal_group_get_target (self->debugger_signals);
}

void
ide_debugger_locals_view_set_debugger (IdeDebuggerLocalsView *self,
                                       IdeDebugger           *debugger)
{
  g_return_if_fail (IDE_IS_DEBUGGER_LOCALS_VIEW (self));
  g_return_if_fail (!debugger || IDE_IS_DEBUGGER (debugger));

  g_signal_group_set_target (self->debugger_signals, debugger);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUGGER]);
}

static void
ide_debugger_locals_view_load_locals_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeDebuggerLocalsView *self;
  IdeDebugger *debugger = (IdeDebugger *)object;
  g_autoptr(GPtrArray) locals = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  GtkTreeIter parent;

  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  locals = ide_debugger_list_locals_finish (debugger, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (locals, g_object_unref);

  if (locals == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_DEBUGGER_LOCALS_VIEW (self));

  gtk_tree_store_append (self->tree_store, &parent, NULL);
  gtk_tree_store_set (self->tree_store, &parent, 1, _("Locals"), -1);

  for (guint i = 0; i < locals->len; i++)
    {
      IdeDebuggerVariable *var = g_ptr_array_index (locals, i);
      GtkTreeIter iter;

      gtk_tree_store_append (self->tree_store, &iter, &parent);
      gtk_tree_store_set (self->tree_store, &iter, 0, var, -1);

      /* Add a deummy row that we can backfill when the user requests
       * that the variable is expanded.
       */
      if (ide_debugger_variable_get_has_children (var))
        {
          GtkTreeIter dummy;

          gtk_tree_store_append (self->tree_store, &dummy, &iter);
        }
    }

  gtk_tree_view_expand_all (self->tree_view);

  ide_task_return_boolean (task, TRUE);
}

static void
ide_debugger_locals_view_load_params_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  g_autoptr(IdeDebuggerLocalsView) self = user_data;
  IdeDebugger *debugger = (IdeDebugger *)object;
  g_autoptr(GPtrArray) params = NULL;
  g_autoptr(GError) error = NULL;
  GtkTreeIter parent;

  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DEBUGGER_LOCALS_VIEW (self));

  params = ide_debugger_list_params_finish (debugger, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (params, g_object_unref);

  if (params == NULL)
    {
      g_warning ("%s", error->message);
      return;
    }

  /* Disposal check */
  if (self->tree_store == NULL)
    return;

  gtk_tree_store_append (self->tree_store, &parent, NULL);
  gtk_tree_store_set (self->tree_store, &parent, 1, _("Parameters"), -1);

  for (guint i = 0; i < params->len; i++)
    {
      IdeDebuggerVariable *var = g_ptr_array_index (params, i);
      GtkTreeIter iter;

      gtk_tree_store_append (self->tree_store, &iter, &parent);
      gtk_tree_store_set (self->tree_store, &iter, 0, var, -1);

      /* Add a deummy row that we can backfill when the user requests
       * that the variable is expanded.
       */
      if (ide_debugger_variable_get_has_children (var))
        {
          GtkTreeIter dummy;

          gtk_tree_store_append (self->tree_store, &dummy, &iter);
        }
    }
}

void
ide_debugger_locals_view_load_async (IdeDebuggerLocalsView *self,
                                     IdeDebuggerThread     *thread,
                                     IdeDebuggerFrame      *frame,
                                     GCancellable          *cancellable,
                                     GAsyncReadyCallback    callback,
                                     gpointer               user_data)
{
  IdeDebugger *debugger;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_DEBUGGER_LOCALS_VIEW (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD (thread));
  g_return_if_fail (IDE_IS_DEBUGGER_FRAME (frame));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  gtk_tree_store_clear (self->tree_store);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, ide_debugger_locals_view_load_async);

  debugger = ide_debugger_locals_view_get_debugger (self);

  if (debugger == NULL)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  ide_debugger_list_params_async (debugger,
                                  thread,
                                  frame,
                                  cancellable,
                                  ide_debugger_locals_view_load_params_cb,
                                  g_object_ref (self));

  ide_debugger_list_locals_async (debugger,
                                  thread,
                                  frame,
                                  cancellable,
                                  ide_debugger_locals_view_load_locals_cb,
                                  g_steal_pointer (&task));
}

gboolean
ide_debugger_locals_view_load_finish (IdeDebuggerLocalsView  *self,
                                      GAsyncResult           *result,
                                      GError                **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_LOCALS_VIEW (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
