/* ide-editor-session-addin.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "ide-editor-session-addin"

#include <gtk/gtk.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-manager.h"
#include "editor/ide-editor-session-addin.h"
#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-view.h"
#include "files/ide-file.h"
#include "layout/ide-layout-grid.h"
#include "layout/ide-layout-grid-column.h"
#include "layout/ide-layout-private.h"
#include "layout/ide-layout-stack.h"
#include "threading/ide-task.h"
#include "workbench/ide-workbench.h"

struct _IdeEditorSessionAddin
{
  IdeObject parent_instance;
};

typedef struct
{
  gchar *uri;
  gint   column;
  gint   row;
  gint   depth;
} Item;

typedef struct
{
  GArray *items;
  gint    active;
} LoadState;

static void
load_state_free (LoadState *state)
{
  g_clear_pointer (&state->items, g_array_unref);
  g_slice_free (LoadState, state);
}

static gint
compare_item (gconstpointer a,
              gconstpointer b)
{
  const Item *item_a = a;
  const Item *item_b = b;
  gint ret;

  if (!(ret = item_a->column - item_b->column))
    {
      if (!(ret = item_a->row - item_b->row))
        ret = item_a->depth - item_b->depth;
    }

  return ret;
}

static void
clear_item (Item *item)
{
  g_clear_pointer (&item->uri, g_free);
}

static void
get_view_position (IdeLayoutView *view,
                   gint          *out_column,
                   gint          *out_row,
                   gint          *out_depth)
{
  GtkWidget *column;
  GtkWidget *grid;
  GtkWidget *lstack;
  GtkWidget *stack;
  gint depth;
  gint index_;

  g_assert (IDE_IS_LAYOUT_VIEW (view));
  g_assert (out_column != NULL);
  g_assert (out_row != NULL);

  *out_column = 0;
  *out_row = 0;
  *out_depth = 0;

  stack = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_STACK);
  lstack = gtk_widget_get_ancestor (GTK_WIDGET (stack), IDE_TYPE_LAYOUT_STACK);
  column = gtk_widget_get_ancestor (GTK_WIDGET (stack), IDE_TYPE_LAYOUT_GRID_COLUMN);
  grid = gtk_widget_get_ancestor (GTK_WIDGET (column), IDE_TYPE_LAYOUT_GRID);

  gtk_container_child_get (GTK_CONTAINER (stack), GTK_WIDGET (view),
                           "position", &depth,
                           NULL);
  *out_depth = MAX (depth, 0);

  gtk_container_child_get (GTK_CONTAINER (column), GTK_WIDGET (lstack),
                           "index", &index_,
                           NULL);
  *out_row = MAX (index_, 0);

  gtk_container_child_get (GTK_CONTAINER (grid), GTK_WIDGET (column),
                           "index", &index_,
                           NULL);
  *out_column = MAX (index_, 0);
}

static void
ide_editor_session_addin_foreach_view_cb (GtkWidget *widget,
                                          gpointer   user_data)
{
  IdeLayoutView *view = (IdeLayoutView *)widget;
  GArray *items = user_data;

  g_assert (IDE_IS_LAYOUT_VIEW (view));
  g_assert (items != NULL);

  if (IDE_IS_EDITOR_VIEW (view))
    {
      IdeBuffer *buffer = ide_editor_view_get_buffer (IDE_EDITOR_VIEW (view));
      IdeFile *file = ide_buffer_get_file (buffer);
      Item item = { 0 };

      if (!ide_file_get_is_temporary (file))
        {
          GFile *gfile = ide_file_get_file (file);

          item.uri = g_file_get_uri (gfile);
          get_view_position (view, &item.column, &item.row, &item.depth);

          IDE_TRACE_MSG ("%u:%u:%u: %s", item.column, item.row, item.depth, item.uri);

          g_array_append_val (items, item);
        }
    }
}

static void
ide_editor_session_addin_save_async (IdeSessionAddin     *addin,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  IdeEditorSessionAddin *self = (IdeEditorSessionAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GArray) items = NULL;
  GVariantBuilder builder;
  IdeContext *context;
  GtkWidget *workbench;

  IDE_ENTRY;

  g_assert (IDE_IS_SESSION_ADDIN (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_editor_session_addin_save_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  workbench = ide_context_get_workbench (context);

  items = g_array_new (FALSE, FALSE, sizeof (Item));
  g_array_set_clear_func (items, (GDestroyNotify)clear_item);

  ide_workbench_views_foreach (IDE_WORKBENCH (workbench),
                               ide_editor_session_addin_foreach_view_cb,
                               items);

  g_array_sort (items, compare_item);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(siii)"));

  for (guint i = 0; i < items->len; i++)
    {
      const Item *item = &g_array_index (items, Item, i);

      g_variant_builder_add (&builder,
                             "(siii)",
                             item->uri,
                             item->column,
                             item->row,
                             item->depth);
    }

  ide_task_return_pointer (task,
                           g_variant_take_ref (g_variant_builder_end (&builder)),
                           (GDestroyNotify)g_variant_unref);

  IDE_EXIT;
}

static GVariant *
ide_editor_session_addin_save_finish (IdeSessionAddin  *self,
                                      GAsyncResult     *result,
                                      GError          **error)
{
  g_assert (IDE_IS_EDITOR_SESSION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_editor_session_addin_load_file_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) loaded = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  LoadState *state;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(loaded = ide_buffer_manager_load_file_finish (bufmgr, result, &error)))
    g_warning ("Failed to load buffer: %s", error->message);

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (state->items != NULL);
  g_assert (state->active > 0);

  state->active--;

  if (state->active == 0)
    {
      IdeContext *context;
      GtkWidget *workbench;
      IdePerspective *editor;
      IdeLayoutGrid *grid;

      context = ide_object_get_context (IDE_OBJECT (bufmgr));
      workbench = ide_context_get_workbench (context);
      editor = ide_workbench_get_perspective_by_name (IDE_WORKBENCH (workbench), "editor");
      grid = ide_editor_perspective_get_grid (IDE_EDITOR_PERSPECTIVE (editor));

      /* Now restore views in the proper place */

      for (guint i = 0; i < state->items->len; i++)
        {
          const Item *item = &g_array_index (state->items, Item, i);
          g_autoptr(GFile) file = NULL;
          IdeLayoutGridColumn *column;
          IdeLayoutStack *stack;
          IdeEditorView *view;
          IdeBuffer *buffer;

          file = g_file_new_for_uri (item->uri);

          if (!(buffer = ide_buffer_manager_find_buffer (bufmgr, file)))
            {
              g_warning ("Failed to restore %s", item->uri);
              continue;
            }

          column = ide_layout_grid_get_nth_column (grid, item->column);
          stack = _ide_layout_grid_get_nth_stack_for_column (grid, column, item->row);

          view = g_object_new (IDE_TYPE_EDITOR_VIEW,
                               "buffer", buffer,
                               "visible", TRUE,
                               NULL);

          gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (view));
        }

      ide_task_return_boolean (task, TRUE);
    }
}

static void
restore_file (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
  IdeFile *file = (IdeFile*)source;
  LoadState *load_state = NULL;
  GError *error = NULL;
  g_autoptr(IdeTask) task = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_FILE (file));
  g_assert (IDE_IS_TASK (task));

  load_state = ide_task_get_task_data (task);

  g_assert (load_state != NULL);

  if (ide_file_exists_finish (file, result, &error))
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (file));
      IdeBufferManager *bufmgr = ide_context_get_buffer_manager (context);

      ide_buffer_manager_load_file_async (bufmgr,
                                          file,
                                          FALSE,
                                          IDE_WORKBENCH_OPEN_FLAGS_NO_VIEW,
                                          NULL,
                                          ide_task_get_cancellable (task),
                                          ide_editor_session_addin_load_file_cb,
                                          g_object_ref (task));
    }
  else
    load_state->active--;

}

static void
ide_editor_session_addin_restore_async (IdeSessionAddin     *addin,
                                        GVariant            *state,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeEditorSessionAddin *self = (IdeEditorSessionAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GHashTable) uris = NULL;
  g_autoptr(GSettings) settings = NULL;
  const gchar *uri;
  LoadState *load_state;
  IdeContext *context;
  GVariantIter iter;
  gint column, row, depth;

  g_assert (IDE_IS_SESSION_ADDIN (addin));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_editor_session_addin_restore_async);

  settings = g_settings_new ("org.gnome.builder");

  if (!g_settings_get_boolean (settings, "restore-previous-files"))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  load_state = g_slice_new0 (LoadState);
  load_state->items = g_array_new (FALSE, FALSE, sizeof (Item));
  g_array_set_clear_func (load_state->items, (GDestroyNotify)clear_item);
  ide_task_set_task_data (task, load_state, load_state_free);

  g_variant_iter_init (&iter, state);

  load_state->active++;

  while (g_variant_iter_next (&iter, "(&siii)", &uri, &column, &row, &depth))
    {
      g_autoptr(IdeFile) file = NULL;
      g_autoptr(GFile) gfile = NULL;
      Item item;

      IDE_TRACE_MSG ("Restore URI \"%s\" at %d:%d:%d", uri, column, row, depth);

      item.uri = g_strdup (uri);
      item.column = column;
      item.row = row;
      item.depth = depth;
      g_array_append_val (load_state->items, item);

      /* Skip loading buffer if already loading */
      if (g_hash_table_contains (uris, uri))
        continue;

      g_hash_table_add (uris, g_strdup (uri));
      gfile = g_file_new_for_uri (uri);
      file = ide_file_new (context, gfile);

      load_state->active++;
      ide_file_exists_async (file,
                             G_PRIORITY_LOW,
                             cancellable,
                             restore_file,
                             g_object_ref(task));
    }

  load_state->active--;

  if (load_state->active == 0)
    ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_editor_session_addin_restore_finish (IdeSessionAddin  *self,
                                         GAsyncResult     *result,
                                         GError          **error)
{
  g_assert (IDE_IS_EDITOR_SESSION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
session_addin_iface_init (IdeSessionAddinInterface *iface)
{
  iface->save_async = ide_editor_session_addin_save_async;
  iface->save_finish = ide_editor_session_addin_save_finish;
  iface->restore_async = ide_editor_session_addin_restore_async;
  iface->restore_finish = ide_editor_session_addin_restore_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeEditorSessionAddin, ide_editor_session_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SESSION_ADDIN, session_addin_iface_init))

static void
ide_editor_session_addin_class_init (IdeEditorSessionAddinClass *klass)
{
}

static void
ide_editor_session_addin_init (IdeEditorSessionAddin *self)
{
}
