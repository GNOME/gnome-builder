/* gbp-editor-session-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-editor-session-addin"

#include "config.h"

#include <libide-code.h>
#include <libide-gui.h>
#include <libide-editor.h>
#include <libide-threading.h>

#include "ide-editor-private.h"
#include "ide-gui-private.h"

#include "gbp-editor-session-addin.h"

struct _GbpEditorSessionAddin
{
  IdeObject parent_instance;
};

typedef struct
{
  gchar *uri;
  gint   column;
  gint   row;
  gint   depth;
  struct {
    gchar    *keyword;
    gboolean  case_sensitive;
    gboolean  regex_enabled;
    gboolean  at_word_boundaries;
  } search;
} Item;

typedef struct
{
  IdeWorkspace *workspace;
  GArray       *items;
  gint          active;
} LoadState;

static void
load_state_free (LoadState *state)
{
  g_clear_pointer (&state->items, g_array_unref);
  g_clear_object (&state->workspace);
  g_slice_free (LoadState, state);
}

static IdeWorkspace *
find_workspace (IdeWorkbench *workbench)
{
  IdeWorkspace *workspace;

  if (!(workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_PRIMARY_WORKSPACE)))
    workspace = ide_workbench_get_workspace_by_type (workbench, IDE_TYPE_EDITOR_WORKSPACE);

  return workspace;
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
  g_clear_pointer (&item->search.keyword, g_free);
}

static void
get_view_position (IdePage *view,
                   gint    *out_column,
                   gint    *out_row,
                   gint    *out_depth)
{
  GtkWidget *column;
  GtkWidget *grid;
  GtkWidget *lstack;
  GtkWidget *stack;
  gint depth;
  gint index_;

  g_assert (IDE_IS_PAGE (view));
  g_assert (out_column != NULL);
  g_assert (out_row != NULL);

  *out_column = 0;
  *out_row = 0;
  *out_depth = 0;

  stack = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_STACK);
  lstack = gtk_widget_get_ancestor (GTK_WIDGET (stack), IDE_TYPE_FRAME);
  column = gtk_widget_get_ancestor (GTK_WIDGET (stack), IDE_TYPE_GRID_COLUMN);
  grid = gtk_widget_get_ancestor (GTK_WIDGET (column), IDE_TYPE_GRID);

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
gbp_editor_session_addin_foreach_page_cb (GtkWidget *widget,
                                          gpointer   user_data)
{
  IdePage *view = (IdePage *)widget;
  GArray *items = user_data;

  g_assert (IDE_IS_PAGE (view));
  g_assert (items != NULL);

  if (IDE_IS_EDITOR_PAGE (view))
    {
      IdeBuffer *buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (view));
      GFile *file = ide_buffer_get_file (buffer);
      IdeEditorSearch *search = ide_editor_page_get_search (IDE_EDITOR_PAGE (view));
      Item item = { 0 };

      if (!ide_buffer_get_is_temporary (buffer))
        {
          item.uri = g_file_get_uri (file);
          get_view_position (view, &item.column, &item.row, &item.depth);

          item.search.keyword = g_strdup (ide_editor_search_get_search_text (search));
          item.search.at_word_boundaries = ide_editor_search_get_at_word_boundaries (search);
          item.search.case_sensitive = ide_editor_search_get_case_sensitive (search);
          item.search.regex_enabled = ide_editor_search_get_regex_enabled (search);

          IDE_TRACE_MSG ("%u:%u:%u: %s", item.column, item.row, item.depth, item.uri);

          g_array_append_val (items, item);
        }
    }
}

static void
gbp_editor_session_addin_save_async (IdeSessionAddin     *addin,
                                     IdeWorkbench        *workbench,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GArray) items = NULL;
  GVariantBuilder builder;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SESSION_ADDIN (addin));
  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_editor_session_addin_save_async);

  items = g_array_new (FALSE, FALSE, sizeof (Item));
  g_array_set_clear_func (items, (GDestroyNotify)clear_item);

  ide_workbench_foreach_page (workbench,
                              gbp_editor_session_addin_foreach_page_cb,
                              items);

  g_array_sort (items, compare_item);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(siiiv)"));

  for (guint i = 0; i < items->len; i++)
    {
      const Item *item = &g_array_index (items, Item, i);
      GVariantBuilder sub;

      g_variant_builder_init (&sub, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add_parsed (&sub, "{'search.keyword',<%s>}",item->search.keyword ?: "");
      g_variant_builder_add_parsed (&sub, "{'search.at-word-boundaries',<%b>}", item->search.at_word_boundaries);
      g_variant_builder_add_parsed (&sub, "{'search.regex-enabled',<%b>}", item->search.regex_enabled);
      g_variant_builder_add_parsed (&sub, "{'search.case-sensitive',<%b>}", item->search.case_sensitive);

      g_variant_builder_add (&builder,
                             "(siiiv)",
                             item->uri,
                             item->column,
                             item->row,
                             item->depth,
                             g_variant_new_variant (g_variant_builder_end (&sub)));
    }

  ide_task_return_pointer (task,
                           g_variant_take_ref (g_variant_builder_end (&builder)),
                           g_variant_unref);

  IDE_EXIT;
}

static GVariant *
gbp_editor_session_addin_save_finish (IdeSessionAddin  *self,
                                      GAsyncResult     *result,
                                      GError          **error)
{
  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
load_state_finish (GbpEditorSessionAddin *self,
                   LoadState             *state)
{
  IdeBufferManager *bufmgr;
  IdeContext *context;
  IdeSurface *editor;
  IdeGrid *grid;

  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (self));
  g_assert (state != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  bufmgr = ide_buffer_manager_from_context (context);

  editor = ide_workspace_get_surface_by_name (state->workspace, "editor");
  grid = ide_editor_surface_get_grid (IDE_EDITOR_SURFACE (editor));

  /* Now restore views in the proper place */

  for (guint i = 0; i < state->items->len; i++)
    {
      const Item *item = &g_array_index (state->items, Item, i);
      g_autoptr(GFile) file = NULL;
      IdeGridColumn *column;
      IdeEditorSearch *search;
      IdeFrame *stack;
      IdeEditorPage *view;
      IdeBuffer *buffer;

      file = g_file_new_for_uri (item->uri);

      if (!(buffer = ide_buffer_manager_find_buffer (bufmgr, file)))
        {
          g_warning ("Failed to restore %s", item->uri);
          continue;
        }

      column = ide_grid_get_nth_column (grid, item->column);
      stack = _ide_grid_get_nth_stack_for_column (grid, column, item->row);

      view = g_object_new (IDE_TYPE_EDITOR_PAGE,
                           "buffer", buffer,
                           "visible", TRUE,
                           NULL);

      search = ide_editor_page_get_search (view);

      ide_editor_search_set_search_text (search, item->search.keyword);
      ide_editor_search_set_at_word_boundaries (search, item->search.at_word_boundaries);
      ide_editor_search_set_case_sensitive (search, item->search.case_sensitive);
      ide_editor_search_set_regex_enabled (search, item->search.regex_enabled);

      gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (view));
    }
}

static void
gbp_editor_session_addin_load_file_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) loaded = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpEditorSessionAddin *self;
  LoadState *state;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(loaded = ide_buffer_manager_load_file_finish (bufmgr, result, &error)))
    g_warning ("Failed to load buffer: %s", error->message);

  state = ide_task_get_task_data (task);
  self = ide_task_get_source_object (task);

  g_assert (state != NULL);
  g_assert (state->items != NULL);
  g_assert (state->active > 0);
  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (self));

  state->active--;

  if (state->active == 0)
    {
      load_state_finish (self, state);
      ide_task_return_boolean (task, TRUE);
    }
}

static void
restore_file (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
  GFile *file = (GFile *)source;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFileInfo) info = NULL;
  GbpEditorSessionAddin *self;
  LoadState *load_state;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  load_state = ide_task_get_task_data (task);

  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (self));
  g_assert (load_state != NULL);

  if ((info = g_file_query_info_finish (file, result, &error)))
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeBufferManager *bufmgr = ide_buffer_manager_from_context (context);

      ide_buffer_manager_load_file_async (bufmgr,
                                          file,
                                          IDE_BUFFER_OPEN_FLAGS_NO_VIEW,
                                          NULL,
                                          ide_task_get_cancellable (task),
                                          gbp_editor_session_addin_load_file_cb,
                                          g_object_ref (task));
    }
  else
    {
      load_state->active--;

      if (load_state->active == 0)
        {
          load_state_finish (self, load_state);
          ide_task_return_boolean (task, TRUE);
        }
    }

  IDE_EXIT;
}

static void
load_task_completed_cb (IdeTask          *task,
                        GParamSpec       *pspec,
                        IdeEditorSurface *surface)
{
  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_EDITOR_SURFACE (surface));

  /* Always show the grid after the task completes */
  _ide_editor_surface_set_loading (surface, FALSE);
}

static void
gbp_editor_session_addin_restore_async (IdeSessionAddin     *addin,
                                        IdeWorkbench        *workbench,
                                        GVariant            *state,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GHashTable) uris = NULL;
  g_autoptr(GSettings) settings = NULL;
  const gchar *uri;
  LoadState *load_state;
  IdeWorkspace *workspace;
  IdeSurface *editor;
  GVariantIter iter;
  GVariant *extra = NULL;
  const gchar *format = "(&siii)";
  gint column, row, depth;

  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (addin));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_editor_session_addin_restore_async);

  if (state == NULL)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  if (!(workspace = find_workspace (workbench)) ||
      !(editor = ide_workspace_get_surface_by_name (workspace, "editor")))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "NO editor surface to restore documents");
      return;
    }

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (load_task_completed_cb),
                           editor,
                           0);

  settings = g_settings_new ("org.gnome.builder");

  if (!g_settings_get_boolean (settings, "restore-previous-files"))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  load_state = g_slice_new0 (LoadState);
  load_state->items = g_array_new (FALSE, FALSE, sizeof (Item));
  load_state->workspace = g_object_ref (workspace);
  g_array_set_clear_func (load_state->items, (GDestroyNotify)clear_item);
  ide_task_set_task_data (task, load_state, load_state_free);

  g_variant_iter_init (&iter, state);

  load_state->active++;

  if (g_variant_is_of_type (state, G_VARIANT_TYPE ("a(siiiv)")))
    format = "(&siiiv)";

  while (g_variant_iter_next (&iter, format, &uri, &column, &row, &depth, &extra))
    {
      g_autoptr(GFile) gfile = NULL;
      Item item = {0};

      IDE_TRACE_MSG ("Restore URI \"%s\" at %d:%d:%d", uri, column, row, depth);

      item.uri = g_strdup (uri);
      item.column = column;
      item.row = row;
      item.depth = depth;

      if (extra != NULL)
        {
          g_autoptr(GVariantDict) dict = NULL;

          dict = g_variant_dict_new (g_variant_get_variant (extra));

          g_variant_dict_lookup (dict, "search.keyword", "s", &item.search.keyword);
          g_variant_dict_lookup (dict, "search.at-word-boundaries", "b", &item.search.at_word_boundaries);
          g_variant_dict_lookup (dict, "search.case-sensitive", "b", &item.search.case_sensitive);
          g_variant_dict_lookup (dict, "search.regex-enabled", "b", &item.search.regex_enabled);
        }

      g_array_append_val (load_state->items, item);

      /* Skip loading buffer if already loading */
      if (!g_hash_table_contains (uris, uri))
        {
          g_hash_table_add (uris, g_strdup (uri));
          gfile = g_file_new_for_uri (uri);

          load_state->active++;

          g_file_query_info_async (gfile,
                                   G_FILE_ATTRIBUTE_STANDARD_NAME,
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_LOW,
                                   cancellable,
                                   restore_file,
                                   g_object_ref (task));
        }

      g_clear_pointer (&extra, g_variant_unref);
    }

  load_state->active--;

  if (load_state->active == 0)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  /* Hide the grid until we've loaded */
  _ide_editor_surface_set_loading (IDE_EDITOR_SURFACE (editor), TRUE);
}

static gboolean
gbp_editor_session_addin_restore_finish (IdeSessionAddin  *self,
                                         GAsyncResult     *result,
                                         GError          **error)
{
  g_assert (GBP_IS_EDITOR_SESSION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
session_addin_iface_init (IdeSessionAddinInterface *iface)
{
  iface->save_async = gbp_editor_session_addin_save_async;
  iface->save_finish = gbp_editor_session_addin_save_finish;
  iface->restore_async = gbp_editor_session_addin_restore_async;
  iface->restore_finish = gbp_editor_session_addin_restore_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpEditorSessionAddin, gbp_editor_session_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SESSION_ADDIN, session_addin_iface_init))

static void
gbp_editor_session_addin_class_init (GbpEditorSessionAddinClass *klass)
{
}

static void
gbp_editor_session_addin_init (GbpEditorSessionAddin *self)
{
}
