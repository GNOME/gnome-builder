/* ide-debugger-threads-view.c
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

#define G_LOG_DOMAIN "ide-debugger-threads-view"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-core.h>
#include <libide-gui.h>

#include "ide-debugger-threads-view.h"

struct _IdeDebuggerThreadsView
{
  AdwBin               parent_instance;

  /* Owned references */
  GSignalGroup        *debugger_signals;

  /* Template References */
  GtkTreeView         *frames_tree_view;
  GtkTreeView         *thread_groups_tree_view;
  GtkTreeView         *threads_tree_view;
  GtkListStore        *frames_store;
  GtkListStore        *thread_groups_store;
  GtkListStore        *threads_store;
  GtkTreeViewColumn   *args_column;
  GtkTreeViewColumn   *binary_column;
  GtkTreeViewColumn   *depth_column;
  GtkTreeViewColumn   *group_column;
  GtkTreeViewColumn   *location_column;
  GtkTreeViewColumn   *thread_column;
  GtkTreeViewColumn   *function_column;
  GtkCellRendererText *args_cell;
  GtkCellRendererText *binary_cell;
  GtkCellRendererText *depth_cell;
  GtkCellRendererText *group_cell;
  GtkCellRendererText *location_cell;
  GtkCellRendererText *thread_cell;
  GtkCellRendererText *function_cell;
};

enum {
  PROP_0,
  PROP_DEBUGGER,
  N_PROPS
};

enum {
  FRAME_ACTIVATED,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (IdeDebuggerThreadsView, ide_debugger_threads_view, ADW_TYPE_BIN)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static IdeDebuggerThread *
ide_debugger_threads_view_get_current_thread (IdeDebuggerThreadsView *self)
{
  g_autoptr(IdeDebuggerThread) thread = NULL;
  GtkTreeSelection *selection;
  GtkTreeModel *model = NULL;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));

  selection = gtk_tree_view_get_selection (self->threads_tree_view);
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter, 0, &thread, -1);

  return g_steal_pointer (&thread);
}

static void
ide_debugger_threads_view_running (IdeDebuggerThreadsView *self,
                                   IdeDebugger            *debugger)
{
  GtkTreeSelection *selection;

  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_list_store_clear (self->frames_store);

  gtk_widget_set_sensitive (GTK_WIDGET (self->frames_tree_view), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->thread_groups_tree_view), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->threads_tree_view), FALSE);

  selection = gtk_tree_view_get_selection (self->threads_tree_view);
  gtk_tree_selection_unselect_all (selection);
}

static void
ide_debugger_threads_view_stopped (IdeDebuggerThreadsView *self,
                                   IdeDebuggerStopReason   stop_reason,
                                   IdeDebuggerBreakpoint  *breakpoint,
                                   IdeDebugger            *debugger)
{
  IdeDebuggerThread *selected;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_STOP_REASON (stop_reason));
  g_assert (!breakpoint || IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_widget_set_sensitive (GTK_WIDGET (self->frames_tree_view), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->thread_groups_tree_view), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->threads_tree_view), TRUE);

  selected = ide_debugger_get_selected_thread (debugger);

  if (selected != NULL)
    {
      model = GTK_TREE_MODEL (self->threads_store);

      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          do
            {
              g_autoptr(IdeDebuggerThread) thread = NULL;

              gtk_tree_model_get (model, &iter, 0, &thread, -1);

              if (ide_debugger_thread_compare (thread, selected) == 0)
                {
                  GtkTreePath *path;

                  selection = gtk_tree_view_get_selection (self->threads_tree_view);
                  gtk_tree_selection_select_iter (selection, &iter);

                  path = gtk_tree_model_get_path (model, &iter);
                  gtk_tree_view_row_activated (self->threads_tree_view, path, self->thread_column);
                  gtk_tree_path_free (path);

                  break;
                }
            }
          while (gtk_tree_model_iter_next (model, &iter));
        }
    }
}

static void
ide_debugger_threads_view_thread_group_added (IdeDebuggerThreadsView *self,
                                              IdeDebuggerThreadGroup *group,
                                              IdeDebugger            *debugger)
{
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_THREAD_GROUP (group));
  g_assert (IDE_IS_DEBUGGER (debugger));

  ide_gtk_list_store_insert_sorted (self->thread_groups_store,
                                    &iter, group, 0,
                                    (GCompareDataFunc)ide_debugger_thread_group_compare,
                                    NULL);
  gtk_list_store_set (self->thread_groups_store, &iter, 0, group, -1);
}

static void
ide_debugger_threads_view_thread_group_removed (IdeDebuggerThreadsView *self,
                                                IdeDebuggerThreadGroup *group,
                                                IdeDebugger            *debugger)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_THREAD_GROUP (group));
  g_assert (IDE_IS_DEBUGGER (debugger));

  model = GTK_TREE_MODEL (self->thread_groups_store);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          g_autoptr(IdeDebuggerThreadGroup) row = NULL;

          gtk_tree_model_get (model, &iter, 0, &row, -1);

          if (ide_debugger_thread_group_compare (row, group) == 0)
            {
              gtk_list_store_remove (self->thread_groups_store, &iter);
              break;
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

static void
ide_debugger_threads_view_thread_added (IdeDebuggerThreadsView *self,
                                        IdeDebuggerThread      *thread,
                                        IdeDebugger            *debugger)
{
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_THREAD (thread));
  g_assert (IDE_IS_DEBUGGER (debugger));

  ide_gtk_list_store_insert_sorted (self->threads_store,
                                    &iter, thread, 0,
                                    (GCompareDataFunc)ide_debugger_thread_compare,
                                    NULL);
  gtk_list_store_set (self->threads_store, &iter, 0, thread, -1);
}

static void
ide_debugger_threads_view_thread_removed (IdeDebuggerThreadsView *self,
                                          IdeDebuggerThread      *thread,
                                          IdeDebugger            *debugger)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_THREAD (thread));
  g_assert (IDE_IS_DEBUGGER (debugger));

  model = GTK_TREE_MODEL (self->threads_store);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          g_autoptr(IdeDebuggerThread) row = NULL;

          gtk_tree_model_get (model, &iter, 0, &row, -1);

          if (ide_debugger_thread_compare (row, thread) == 0)
            {
              gtk_list_store_remove (self->threads_store, &iter);
              break;
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

static void
ide_debugger_threads_view_list_frames_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeDebugger *debugger = (IdeDebugger *)object;
  g_autoptr(IdeDebuggerThreadsView) self = user_data;
  g_autoptr(GPtrArray) frames = NULL;
  g_autoptr(GError) error = NULL;
  GtkTreeSelection *selection;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  frames = ide_debugger_list_frames_finish (debugger, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (frames, g_object_unref);

  if (frames == NULL)
    {
      g_warning ("%s", error->message);
      return;
    }

  gtk_list_store_clear (self->frames_store);

  for (guint i = 0; i < frames->len; i++)
    {
      IdeDebuggerFrame *frame = g_ptr_array_index (frames, i);

      g_assert (IDE_IS_DEBUGGER_FRAME (frame));

      gtk_list_store_append (self->frames_store, &iter);
      gtk_list_store_set (self->frames_store, &iter, 0, frame, -1);
    }

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->frames_store), &iter))
    {
      GtkTreePath *path;

      selection = gtk_tree_view_get_selection (self->frames_tree_view);
      gtk_tree_selection_select_iter (selection, &iter);

      path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->frames_store), &iter);
      gtk_tree_view_row_activated (self->frames_tree_view, path, self->depth_column);
      gtk_tree_path_free (path);
    }
}

static void
ide_debugger_threads_view_bind (IdeDebuggerThreadsView *self,
                                IdeDebugger            *debugger,
                                GSignalGroup           *debugger_signals)
{
  GListModel *thread_groups;
  GListModel *threads;
  guint n_items;

  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (G_IS_SIGNAL_GROUP (debugger_signals));

  /* Add any thread groups already loaded by the debugger */

  thread_groups = ide_debugger_get_thread_groups (debugger);
  n_items = g_list_model_get_n_items (thread_groups);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeDebuggerThreadGroup) group = NULL;

      group = g_list_model_get_item (thread_groups, i);
      ide_debugger_threads_view_thread_group_added (self, group, debugger);
    }

  /* Add any threads already loaded by the debugger */

  threads = ide_debugger_get_threads (debugger);
  n_items = g_list_model_get_n_items (threads);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeDebuggerThread) thread = NULL;

      thread = g_list_model_get_item (threads, i);
      ide_debugger_threads_view_thread_added (self, thread, debugger);
    }
}

static void
ide_debugger_threads_view_unbind (IdeDebuggerThreadsView *self,
                                  GSignalGroup           *debugger_signals)
{
  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (G_IS_SIGNAL_GROUP (debugger_signals));

  gtk_list_store_clear (self->thread_groups_store);
  gtk_list_store_clear (self->threads_store);
  gtk_list_store_clear (self->frames_store);
}

static void
argv_property_cell_data_func (GtkCellLayout   *cell_layout,
                              GtkCellRenderer *cell,
                              GtkTreeModel    *model,
                              GtkTreeIter     *iter,
                              gpointer         user_data)
{
  const gchar *property = user_data;
  g_autoptr(GObject) object = NULL;
  g_auto(GStrv) strv = NULL;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (property != NULL);

  gtk_tree_model_get (model, iter, 0, &object, -1);

  if (object != NULL)
    {
      g_object_get (object, property, &strv, NULL);

      if (strv != NULL)
        {
          g_autoptr(GString) str = g_string_new (NULL);

          g_string_append_c (str, '(');
          for (guint i = 0; strv[i]; i++)
            {
              g_string_append (str, strv[i]);
              if (strv[i+1])
                g_string_append (str, ", ");
            }
          g_string_append_c (str, ')');

          g_object_set (cell, "text", str->str, NULL);

          return;
        }
    }

  g_object_set (cell, "text", "", NULL);
}

static void
location_property_cell_data_func (GtkCellLayout   *cell_layout,
                                  GtkCellRenderer *cell,
                                  GtkTreeModel    *model,
                                  GtkTreeIter     *iter,
                                  gpointer         user_data)
{
  g_autoptr(IdeDebuggerFrame) frame = NULL;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  gtk_tree_model_get (model, iter, 0, &frame, -1);

  if (frame != NULL)
    {
      const gchar *file = ide_debugger_frame_get_file (frame);
      guint line = ide_debugger_frame_get_line (frame);

      if (file != NULL)
        {
          if (line != 0)
            {
              g_autofree gchar *text = NULL;

              text = g_strdup_printf ("%s<span fgalpha='32767'>:%u</span>", file, line);
              g_object_set (cell, "markup", text, NULL);
            }
          else
            {
              g_object_set (cell, "text", file, NULL);
            }

          return;
        }
    }

  g_object_set (cell, "text", NULL, NULL);
}

static void
binary_property_cell_data_func (GtkCellLayout   *cell_layout,
                                GtkCellRenderer *cell,
                                GtkTreeModel    *model,
                                GtkTreeIter     *iter,
                                gpointer         user_data)
{
  IdeDebuggerThreadsView *self = user_data;
  IdeDebugger *debugger;
  g_autoptr(IdeDebuggerFrame) frame = NULL;

  g_assert (GTK_IS_TREE_VIEW_COLUMN (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  debugger = _g_signal_group_get_target (self->debugger_signals);
  if (debugger == NULL)
    return;

  gtk_tree_model_get (model, iter, 0, &frame, -1);

  if (frame != NULL)
    {
      IdeDebuggerAddress address;
      const gchar *name;

      address = ide_debugger_frame_get_address (frame);
      name = ide_debugger_locate_binary_at_address (debugger, address);
      g_object_set (cell, "text", name, NULL);
      return;
    }

  g_object_set (cell, "text", NULL, NULL);
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
ide_debugger_threads_view_threads_row_activated (IdeDebuggerThreadsView *self,
                                                 GtkTreePath            *path,
                                                 GtkTreeViewColumn      *column,
                                                 GtkTreeView            *tree_view)
{
  IdeDebugger *debugger;
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (path != NULL);
  g_assert (GTK_IS_TREE_VIEW_COLUMN (column));
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  model = gtk_tree_view_get_model (tree_view);
  debugger = _g_signal_group_get_target (self->debugger_signals);

  if (debugger == NULL)
    return;

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      g_autoptr(IdeDebuggerThread) thread = NULL;

      gtk_tree_model_get (model, &iter, 0, &thread, -1);
      g_assert (!thread || IDE_IS_DEBUGGER_THREAD (thread));

      if (thread != NULL)
        {
          ide_debugger_list_frames_async (debugger,
                                          thread,
                                          NULL,
                                          ide_debugger_threads_view_list_frames_cb,
                                          g_object_ref (self));
        }
    }
}

static void
ide_debugger_threads_view_frames_row_activated (IdeDebuggerThreadsView *self,
                                                GtkTreePath            *path,
                                                GtkTreeViewColumn      *column,
                                                GtkTreeView            *tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_assert (path != NULL);
  g_assert (GTK_IS_TREE_VIEW_COLUMN (column));
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      g_autoptr(IdeDebuggerFrame) frame = NULL;

      gtk_tree_model_get (model, &iter, 0, &frame, -1);

      if (frame != NULL)
        {
          g_autoptr(IdeDebuggerThread) thread = NULL;

          thread = ide_debugger_threads_view_get_current_thread (self);
          if (thread != NULL && frame != NULL)
            g_signal_emit (self, signals [FRAME_ACTIVATED], 0, thread, frame);
        }
    }
}

static void
ide_debugger_threads_view_dispose (GObject *object)
{
  IdeDebuggerThreadsView *self = (IdeDebuggerThreadsView *)object;

  g_clear_object (&self->debugger_signals);

  G_OBJECT_CLASS (ide_debugger_threads_view_parent_class)->dispose (object);
}

static void
ide_debugger_threads_view_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeDebuggerThreadsView *self = IDE_DEBUGGER_THREADS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      g_value_set_object (value, ide_debugger_threads_view_get_debugger (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_threads_view_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeDebuggerThreadsView *self = IDE_DEBUGGER_THREADS_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      ide_debugger_threads_view_set_debugger (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_threads_view_class_init (IdeDebuggerThreadsViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_debugger_threads_view_dispose;
  object_class->get_property = ide_debugger_threads_view_get_property;
  object_class->set_property = ide_debugger_threads_view_set_property;

  properties [PROP_DEBUGGER] =
    g_param_spec_object ("debugger",
                         "Debugger",
                         "Debugger",
                         IDE_TYPE_DEBUGGER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [FRAME_ACTIVATED] =
    g_signal_new ("frame-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, IDE_TYPE_DEBUGGER_THREAD, IDE_TYPE_DEBUGGER_FRAME);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/debuggerui/ide-debugger-threads-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, args_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, args_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, binary_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, binary_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, depth_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, depth_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, frames_store);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, frames_tree_view);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, function_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, function_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, group_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, group_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, location_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, location_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, thread_cell);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, thread_column);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, thread_groups_store);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, thread_groups_tree_view);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, threads_store);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerThreadsView, threads_tree_view);

  g_type_ensure (IDE_TYPE_DEBUGGER_FRAME);
  g_type_ensure (IDE_TYPE_DEBUGGER_THREAD);
  g_type_ensure (IDE_TYPE_DEBUGGER_THREAD_GROUP);
}

static void
ide_debugger_threads_view_init (IdeDebuggerThreadsView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->debugger_signals = g_signal_group_new (IDE_TYPE_DEBUGGER);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "running",
                                    G_CALLBACK (ide_debugger_threads_view_running),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "stopped",
                                    G_CALLBACK (ide_debugger_threads_view_stopped),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "thread-group-added",
                                    G_CALLBACK (ide_debugger_threads_view_thread_group_added),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "thread-group-removed",
                                    G_CALLBACK (ide_debugger_threads_view_thread_group_removed),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "thread-added",
                                    G_CALLBACK (ide_debugger_threads_view_thread_added),
                                    self);

  g_signal_group_connect_swapped (self->debugger_signals,
                                    "thread-removed",
                                    G_CALLBACK (ide_debugger_threads_view_thread_removed),
                                    self);

  g_signal_connect_swapped (self->debugger_signals,
                            "bind",
                            G_CALLBACK (ide_debugger_threads_view_bind),
                            self);

  g_signal_connect_swapped (self->debugger_signals,
                            "unbind",
                            G_CALLBACK (ide_debugger_threads_view_unbind),
                            self);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->group_column),
                                      GTK_CELL_RENDERER (self->group_cell),
                                      string_property_cell_data_func, (gchar *)"id", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->thread_column),
                                      GTK_CELL_RENDERER (self->thread_cell),
                                      string_property_cell_data_func, (gchar *)"id", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->depth_column),
                                      GTK_CELL_RENDERER (self->depth_cell),
                                      int_property_cell_data_func, (gchar *)"depth", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->function_column),
                                      GTK_CELL_RENDERER (self->function_cell),
                                      string_property_cell_data_func, (gchar *)"function", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->args_column),
                                      GTK_CELL_RENDERER (self->args_cell),
                                      argv_property_cell_data_func, (gchar *)"args", NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->location_column),
                                      GTK_CELL_RENDERER (self->location_cell),
                                      location_property_cell_data_func, (gchar *)NULL, NULL);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->binary_column),
                                      GTK_CELL_RENDERER (self->binary_cell),
                                      binary_property_cell_data_func, self, NULL);

  g_signal_connect_swapped (self->threads_tree_view,
                            "row-activated",
                            G_CALLBACK (ide_debugger_threads_view_threads_row_activated),
                            self);

  g_signal_connect_swapped (self->frames_tree_view,
                            "row-activated",
                            G_CALLBACK (ide_debugger_threads_view_frames_row_activated),
                            self);
}

/**
 * ide_debugger_threads_view_get_debugger:
 * @self: a #IdeDebuggerThreadsView
 *
 * Gets the debugger that is being observed.
 *
 * Returns: (transfer none) (nullable): An #IdeDebugger or %NULL
 */
IdeDebugger *
ide_debugger_threads_view_get_debugger (IdeDebuggerThreadsView *self)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_THREADS_VIEW (self), NULL);

  return _g_signal_group_get_target (self->debugger_signals);
}

void
ide_debugger_threads_view_set_debugger (IdeDebuggerThreadsView *self,
                                        IdeDebugger            *debugger)
{
  g_return_if_fail (IDE_IS_DEBUGGER_THREADS_VIEW (self));
  g_return_if_fail (!debugger || IDE_IS_DEBUGGER (debugger));

  g_signal_group_set_target (self->debugger_signals, debugger);
}
