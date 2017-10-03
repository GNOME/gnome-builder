/* ide-layout-stack-actions.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include "layout/ide-layout-stack.h"
#include "layout/ide-layout-private.h"

#include <ide.h>

static void
ide_layout_stack_actions_next_view (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  IdeLayoutStack *self = user_data;

  g_assert (IDE_IS_LAYOUT_STACK (self));

  g_signal_emit_by_name (self, "change-current-page", 1);
}

static void
ide_layout_stack_actions_previous_view (GSimpleAction *action,
                                        GVariant      *variant,
                                        gpointer       user_data)
{
  IdeLayoutStack *self = user_data;

  g_assert (IDE_IS_LAYOUT_STACK (self));

  g_signal_emit_by_name (self, "change-current-page", -1);
}

static void
ide_layout_stack_actions_close_view (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeLayoutStack *self = user_data;
  IdeLayoutView *view;

  g_assert (IDE_IS_LAYOUT_STACK (self));

  view = ide_layout_stack_get_visible_child (self);
  if (view != NULL)
    _ide_layout_stack_request_close (self, view);
}

static void
ide_layout_stack_actions_move (IdeLayoutStack *self,
                               gint            direction)
{
  IdeLayoutView *view;
  IdeLayoutStack *dest;
  GtkWidget *grid;
  GtkWidget *column;
  gint index = 0;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (direction == 1 || direction == -1);

  view = ide_layout_stack_get_visible_child (self);

  g_return_if_fail (view != NULL);
  g_return_if_fail (IDE_IS_LAYOUT_VIEW (view));

  grid = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_LAYOUT_GRID);
  g_return_if_fail (grid != NULL);
  g_return_if_fail (IDE_IS_LAYOUT_GRID (grid));

  column = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_LAYOUT_GRID_COLUMN);
  g_return_if_fail (column != NULL);
  g_return_if_fail (IDE_IS_LAYOUT_GRID_COLUMN (column));

  gtk_container_child_get (GTK_CONTAINER (grid), GTK_WIDGET (column),
                           "index", &index,
                           NULL);

  dest = _ide_layout_grid_get_nth_stack (IDE_LAYOUT_GRID (grid), index + direction);

  g_return_if_fail (dest != NULL);
  g_return_if_fail (dest != self);
  g_return_if_fail (IDE_IS_LAYOUT_STACK (dest));

  _ide_layout_stack_transfer (self, dest, view);
}

static void
ide_layout_stack_actions_move_right (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeLayoutStack *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_LAYOUT_STACK (self));

  ide_layout_stack_actions_move (self, 1);
}

static void
ide_layout_stack_actions_move_left (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  IdeLayoutStack *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_LAYOUT_STACK (self));

  ide_layout_stack_actions_move (self, -1);
}

static void
ide_layout_stack_actions_open_in_new_frame (GSimpleAction *action,
                                            GVariant      *variant,
                                            gpointer       user_data)
{
  IdeLayoutStack *self = user_data;
  const gchar *filepath;
  IdeLayoutView *view;
  IdeLayoutStack *dest;
  GtkWidget *grid;
  GtkWidget *column;
  gint index = 0;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));

  filepath = g_variant_get_string (variant, NULL);
  if (!ide_str_empty0 (filepath))
    {
      IdeContext *context;
      IdeBufferManager *buffer_manager;
      g_autoptr (GFile) file = NULL;
      IdeBuffer *buffer;

      context = ide_widget_get_context (GTK_WIDGET (self));
      buffer_manager = ide_context_get_buffer_manager (context);
      file = g_file_new_for_path (filepath);
      if (NULL != (buffer = ide_buffer_manager_find_buffer (buffer_manager, file)))
        view = g_object_new (IDE_TYPE_EDITOR_VIEW,
                             "buffer", buffer,
                             "visible", TRUE,
                             NULL);
      else
        return;
    }
  else
    {
      view = ide_layout_stack_get_visible_child (self);

      g_return_if_fail (view != NULL);
      g_return_if_fail (IDE_IS_LAYOUT_VIEW (view));
      g_return_if_fail (ide_layout_view_get_can_split (view));

      view = ide_layout_view_create_split_view (view);
    }

  if (view == NULL)
    {
      g_warning ("Requested split view but NULL was returned");
      return;
    }

  grid = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_LAYOUT_GRID);

  g_return_if_fail (grid != NULL);
  g_return_if_fail (IDE_IS_LAYOUT_GRID (grid));

  column = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_LAYOUT_GRID_COLUMN);

  g_return_if_fail (column != NULL);
  g_return_if_fail (IDE_IS_LAYOUT_GRID_COLUMN (column));

  gtk_container_child_get (GTK_CONTAINER (grid), GTK_WIDGET (column),
                           "index", &index,
                           NULL);

  dest = _ide_layout_grid_get_nth_stack (IDE_LAYOUT_GRID (grid), ++index);

  g_return_if_fail (dest != NULL);
  g_return_if_fail (IDE_IS_LAYOUT_STACK (dest));

  gtk_container_add (GTK_CONTAINER (dest), GTK_WIDGET (view));
}

static void
ide_layout_stack_actions_split_view (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeLayoutStack *self = user_data;
  IdeLayoutStack *dest;
  IdeLayoutView *view;
  IdeLayoutView *split_view;
  const gchar *filepath;
  GtkWidget *column;
  GtkWidget *grid;
  gint index = 0;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));

  column = gtk_widget_get_parent (GTK_WIDGET (self));

  if (column == NULL || !IDE_IS_LAYOUT_GRID_COLUMN (column))
    {
      g_warning ("Failed to locate ancestor grid column");
      return;
    }

  grid = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_LAYOUT_GRID);

  if (grid == NULL)
    {
      g_warning ("Failed to locate ancestor grid");
      return;
    }

  filepath = g_variant_get_string (variant, NULL);
  if (!ide_str_empty0 (filepath))
    {
      IdeContext *context;
      IdeBufferManager *buffer_manager;
      g_autoptr (GFile) file = NULL;
      IdeBuffer *buffer;

      context = ide_widget_get_context (GTK_WIDGET (self));
      buffer_manager = ide_context_get_buffer_manager (context);
      file = g_file_new_for_path (filepath);
      if (NULL != (buffer = ide_buffer_manager_find_buffer (buffer_manager, file)))
        split_view = g_object_new (IDE_TYPE_EDITOR_VIEW,
                                  "buffer", buffer,
                                  "visible", TRUE,
                                  NULL);
      else
        return;
    }
  else
    {
      view = ide_layout_stack_get_visible_child (self);
      if (view == NULL)
        {
          g_warning ("No view available to split");
          return;
        }

      if (!ide_layout_view_get_can_split (view))
        {
          g_warning ("Attempt to split a view that cannot be split");
          return;
        }

        split_view = ide_layout_view_create_split_view (view);
    }

  g_assert (IDE_IS_LAYOUT_GRID_COLUMN (column));

  gtk_container_child_get (GTK_CONTAINER (column), GTK_WIDGET (self),
                           "index", &index,
                           NULL);

  dest = _ide_layout_grid_get_nth_stack_for_column (IDE_LAYOUT_GRID (grid),
                                                    IDE_LAYOUT_GRID_COLUMN (column),
                                                    ++index);

  g_assert (IDE_IS_LAYOUT_STACK (dest));

  gtk_container_add (GTK_CONTAINER (dest), GTK_WIDGET (split_view));
}

static void
ide_layout_stack_actions_close_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeLayoutStack *self = (IdeLayoutStack *)object;
  GtkWidget *parent;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_layout_stack_agree_to_close_finish (self, result, NULL))
    return;

  /* Things might have changed during the async op */
  parent = gtk_widget_get_parent (GTK_WIDGET (self));
  if (!IDE_IS_LAYOUT_GRID_COLUMN (parent))
    return;

  /* Make sure there is still more than a single stack */
  if (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (parent)) > 1)
    gtk_widget_destroy (GTK_WIDGET (self));
}

static void
ide_layout_stack_actions_close_stack (GSimpleAction *action,
                                      GVariant      *variant,
                                      gpointer       user_data)
{
  IdeLayoutStack *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_LAYOUT_STACK (self));

  ide_layout_stack_agree_to_close_async (self,
                                         NULL,
                                         ide_layout_stack_actions_close_cb,
                                         NULL);
}

static void
ide_layout_stack_actions_show_list (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  IdeLayoutStack *self = user_data;
  IdeLayoutStackHeader *header;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_LAYOUT_STACK (self));

  header = IDE_LAYOUT_STACK_HEADER (ide_layout_stack_get_titlebar (self));
  _ide_layout_stack_header_focus_list (header);
}

static const GActionEntry actions[] = {
  { "open-in-new-frame", ide_layout_stack_actions_open_in_new_frame, "s" },
  { "close-stack",       ide_layout_stack_actions_close_stack },
  { "close-view",        ide_layout_stack_actions_close_view },
  { "next-view",         ide_layout_stack_actions_next_view },
  { "previous-view",     ide_layout_stack_actions_previous_view },
  { "move-right",        ide_layout_stack_actions_move_right },
  { "move-left",         ide_layout_stack_actions_move_left },
  { "split-view",        ide_layout_stack_actions_split_view, "s" },
  { "show-list",         ide_layout_stack_actions_show_list },
};

void
_ide_layout_stack_update_actions (IdeLayoutStack *self)
{
  IdeLayoutView *view;
  GtkWidget *parent;
  gboolean has_view = FALSE;
  gboolean can_split_view = FALSE;
  gboolean can_close_stack = FALSE;

  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));

  view = ide_layout_stack_get_visible_child (self);

  if (view != NULL)
    {
      has_view = TRUE;
      can_split_view = ide_layout_view_get_can_split (view);
    }

  /* If there is more than one stack in the column, then we can close
   * this stack directly without involving the column.
   */
  parent = gtk_widget_get_parent (GTK_WIDGET (self));
  if (IDE_IS_LAYOUT_GRID_COLUMN (parent))
    can_close_stack = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (parent)) > 1;

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "layoutstack", "move-right",
                             "enabled", has_view,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "layoutstack", "move-left",
                             "enabled", has_view,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "layoutstack", "open-in-new-frame",
                             "enabled", can_split_view,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "layoutstack", "split-view",
                             "enabled", can_split_view,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "layoutstack", "close-stack",
                             "enabled", can_close_stack,
                             NULL);
}

void
_ide_layout_stack_init_actions (IdeLayoutStack *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "layoutstack",
                                  G_ACTION_GROUP (group));

  _ide_layout_stack_update_actions (self);
}
