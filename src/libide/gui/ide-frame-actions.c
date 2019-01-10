/* ide-frame-actions.c
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

#define G_LOG_DOMAIN "ide-frame-actions"

#include "config.h"

#include "ide-frame.h"
#include "ide-gui-global.h"
#include "ide-gui-private.h"
#include "ide-workbench.h"

static void
ide_frame_actions_next_page (GSimpleAction *action,
                             GVariant      *variant,
                             gpointer       user_data)
{
  IdeFrame *self = user_data;

  g_assert (IDE_IS_FRAME (self));

  g_signal_emit_by_name (self, "change-current-page", 1);
}

static void
ide_frame_actions_previous_page (GSimpleAction *action,
                                 GVariant      *variant,
                                 gpointer       user_data)
{
  IdeFrame *self = user_data;

  g_assert (IDE_IS_FRAME (self));

  g_signal_emit_by_name (self, "change-current-page", -1);
}

static void
ide_frame_actions_close_page (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  IdeFrame *self = user_data;
  IdePage *page;

  g_assert (IDE_IS_FRAME (self));

  page = ide_frame_get_visible_child (self);
  if (page != NULL)
    _ide_frame_request_close (self, page);
}

static void
ide_frame_actions_move (IdeFrame *self,
                        gint      direction)
{
  IdePage *page;
  IdeFrame *dest;
  GtkWidget *grid;
  GtkWidget *column;
  gint index = 0;

  g_assert (IDE_IS_FRAME (self));
  g_assert (direction == 1 || direction == -1);

  page = ide_frame_get_visible_child (self);

  g_return_if_fail (page != NULL);
  g_return_if_fail (IDE_IS_PAGE (page));

  grid = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GRID);
  g_return_if_fail (grid != NULL);
  g_return_if_fail (IDE_IS_GRID (grid));

  column = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GRID_COLUMN);
  g_return_if_fail (column != NULL);
  g_return_if_fail (IDE_IS_GRID_COLUMN (column));

  gtk_container_child_get (GTK_CONTAINER (grid), GTK_WIDGET (column),
                           "index", &index,
                           NULL);

  dest = _ide_grid_get_nth_stack (IDE_GRID (grid), index + direction);

  g_return_if_fail (dest != NULL);
  g_return_if_fail (dest != self);
  g_return_if_fail (IDE_IS_FRAME (dest));

  _ide_frame_transfer (self, dest, page);
}

static void
ide_frame_actions_move_right (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  IdeFrame *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_FRAME (self));

  ide_frame_actions_move (self, 1);
}

static void
ide_frame_actions_move_left (GSimpleAction *action,
                             GVariant      *variant,
                             gpointer       user_data)
{
  IdeFrame *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_FRAME (self));

  ide_frame_actions_move (self, -1);
}

static void
ide_frame_actions_split_page (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  IdeFrame *self = user_data;
  g_autoptr(GFile) file = NULL;
  IdeBufferManager *bufmgr;
  GObjectClass *klass;
  const gchar *path;
  GParamSpec *pspec;
  IdeContext *context;
  IdeBuffer *buffer;
  GtkWidget *column;
  GtkWidget *grid;
  IdeFrame *dest;
  IdePage *page;
  IdePage *split_page;
  gint index = 0;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_FRAME (self));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));

  column = gtk_widget_get_parent (GTK_WIDGET (self));

  if (column == NULL || !IDE_IS_GRID_COLUMN (column))
    {
      g_warning ("Failed to locate ancestor grid column");
      return;
    }

  if (!(grid = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GRID)))
    {
      g_warning ("Failed to locate ancestor grid");
      return;
    }

  if (!(page = ide_frame_get_visible_child (self)))
    {
      g_warning ("No page available to split");
      return;
    }

  if ((path = g_variant_get_string (variant, NULL)) &&
      !ide_str_empty0 (path) &&
      (context = ide_widget_get_context (GTK_WIDGET (self))) &&
      (bufmgr = ide_buffer_manager_from_context (context)) &&
      (file = g_file_new_for_path (path)) &&
      (buffer = ide_buffer_manager_find_buffer (bufmgr, file)) &&
      (klass = G_OBJECT_GET_CLASS (page)) &&
      (pspec = g_object_class_find_property (klass, "buffer")) &&
      g_type_is_a (pspec->value_type, IDE_TYPE_BUFFER))
    {
      split_page = g_object_new (G_OBJECT_TYPE (page),
                                 "buffer", buffer,
                                 "visible", TRUE,
                                 NULL);
    }
  else
    {
      if (!ide_page_get_can_split (page))
        {
          g_warning ("Attempt to split a page that cannot be split");
          return;
        }

      if (!(split_page = ide_page_create_split (page)))
        {
          g_warning ("%s failed to create a split",
                     G_OBJECT_TYPE_NAME (page));
          return;
        }
    }

  g_assert (IDE_IS_PAGE (split_page));
  g_assert (IDE_IS_GRID_COLUMN (column));

  gtk_container_child_get (GTK_CONTAINER (column), GTK_WIDGET (self),
                           "index", &index,
                           NULL);

  dest = _ide_grid_get_nth_stack_for_column (IDE_GRID (grid),
                                             IDE_GRID_COLUMN (column),
                                             ++index);

  g_assert (IDE_IS_FRAME (dest));

  gtk_container_add (GTK_CONTAINER (dest), GTK_WIDGET (split_page));
}

static void
ide_frame_actions_open_in_new_frame (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeFrame *self = user_data;
  const gchar *filepath;
  GtkWidget *grid;
  GtkWidget *column;
  IdeFrame *dest;
  IdePage *page;
  gint index = 0;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_FRAME (self));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));

  filepath = g_variant_get_string (variant, NULL);
  page = ide_frame_get_visible_child (self);

  g_return_if_fail (page != NULL);
  g_return_if_fail (IDE_IS_PAGE (page));

  if (!ide_str_empty0 (filepath))
    {
      g_autoptr (GFile) file = NULL;
      IdeBufferManager *buffer_manager;
      IdeContext *context;
      IdeBuffer *buffer;

      context = ide_widget_get_context (GTK_WIDGET (self));
      buffer_manager = ide_buffer_manager_from_context (context);
      file = g_file_new_for_path (filepath);

      if ((buffer = ide_buffer_manager_find_buffer (buffer_manager, file)))
        page = g_object_new (G_OBJECT_TYPE (page),
                             "buffer", buffer,
                             "visible", TRUE,
                             NULL);
      else
        return;
    }
  else
    {
      g_return_if_fail (ide_page_get_can_split (page));

      page = ide_page_create_split (page);
    }

  if (page == NULL)
    {
      g_warning ("Requested split page but NULL was returned");
      return;
    }

  grid = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GRID);

  g_return_if_fail (grid != NULL);
  g_return_if_fail (IDE_IS_GRID (grid));

  column = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GRID_COLUMN);

  g_return_if_fail (column != NULL);
  g_return_if_fail (IDE_IS_GRID_COLUMN (column));

  gtk_container_child_get (GTK_CONTAINER (grid), GTK_WIDGET (column),
                           "index", &index,
                           NULL);

  dest = _ide_grid_get_nth_stack (IDE_GRID (grid), ++index);

  g_return_if_fail (dest != NULL);
  g_return_if_fail (IDE_IS_FRAME (dest));

  gtk_container_add (GTK_CONTAINER (dest), GTK_WIDGET (page));
}

static void
ide_frame_actions_close_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeFrame *self = (IdeFrame *)object;
  GtkWidget *parent;

  g_assert (IDE_IS_FRAME (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_frame_agree_to_close_finish (self, result, NULL))
    return;

  /* Things might have changed during the async op */
  parent = gtk_widget_get_parent (GTK_WIDGET (self));
  if (!IDE_IS_GRID_COLUMN (parent))
    return;

  /* Make sure there is still more than a single stack */
  if (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (parent)) > 1)
    gtk_widget_destroy (GTK_WIDGET (self));
}

static void
ide_frame_actions_close_stack (GSimpleAction *action,
                               GVariant      *variant,
                               gpointer       user_data)
{
  IdeFrame *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_FRAME (self));

  ide_frame_agree_to_close_async (self,
                                         NULL,
                                         ide_frame_actions_close_cb,
                                         NULL);
}

static void
ide_frame_actions_show_list (GSimpleAction *action,
                             GVariant      *variant,
                             gpointer       user_data)
{
  IdeFrame *self = user_data;
  IdeFrameHeader *header;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_FRAME (self));

  header = IDE_FRAME_HEADER (ide_frame_get_titlebar (self));
  _ide_frame_header_focus_list (header);
}

static const GActionEntry actions[] = {
  { "open-in-new-frame", ide_frame_actions_open_in_new_frame, "s" },
  { "close-stack",       ide_frame_actions_close_stack },
  { "close-page",        ide_frame_actions_close_page },
  { "next-page",         ide_frame_actions_next_page },
  { "previous-page",     ide_frame_actions_previous_page },
  { "move-right",        ide_frame_actions_move_right },
  { "move-left",         ide_frame_actions_move_left },
  { "split-page",        ide_frame_actions_split_page, "s" },
  { "show-list",         ide_frame_actions_show_list },
};

void
_ide_frame_update_actions (IdeFrame *self)
{
  IdePage *page;
  GtkWidget *parent;
  gboolean has_page = FALSE;
  gboolean can_split_page = FALSE;
  gboolean can_close_stack = FALSE;

  g_return_if_fail (IDE_IS_FRAME (self));

  page = ide_frame_get_visible_child (self);

  if (page != NULL)
    {
      has_page = TRUE;
      can_split_page = ide_page_get_can_split (page);
    }

  /* If there is more than one stack in the column, then we can close
   * this stack directly without involving the column.
   */
  parent = gtk_widget_get_parent (GTK_WIDGET (self));
  if (IDE_IS_GRID_COLUMN (parent))
    can_close_stack = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (parent)) > 1;

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "frame", "move-right",
                             "enabled", has_page,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "frame", "move-left",
                             "enabled", has_page,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "frame", "open-in-new-frame",
                             "enabled", can_split_page,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "frame", "split-page",
                             "enabled", can_split_page,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "frame", "close-stack",
                             "enabled", can_close_stack,
                             NULL);
}

void
_ide_frame_init_actions (IdeFrame *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_return_if_fail (IDE_IS_FRAME (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "frame",
                                  G_ACTION_GROUP (group));

  _ide_frame_update_actions (self);
}
