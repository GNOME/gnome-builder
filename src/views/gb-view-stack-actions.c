/* gb-view-stack-actions.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "gb-view-stack"

#include "gb-view.h"
#include "gb-view-grid.h"
#include "gb-view-stack.h"
#include "gb-view-stack-actions.h"
#include "gb-view-stack-private.h"

static void
gb_view_stack_actions_close_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GbViewStack *self = (GbViewStack *)object;
  g_autoptr(GbView) view = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
  g_assert (GB_IS_VIEW (view));

  gb_view_stack_remove (self, view);
}


static void
gb_view_stack_actions_close (GSimpleAction *action,
                             GVariant      *param,
                             gpointer       user_data)
{
  g_autoptr(GTask) task = NULL;
  GbViewStack *self = user_data;
  GtkWidget *active_view;

  g_assert (GB_IS_VIEW_STACK (self));

  active_view = gb_view_stack_get_active_view (self);
  if (active_view == NULL || !GB_IS_VIEW (active_view))
    return;


  /*
   * Queue until we are out of this potential signalaction. (Which mucks things
   * up since it expects the be able to continue working with the widget).
   */
  task = g_task_new (self, NULL, gb_view_stack_actions_close_cb, g_object_ref (active_view));
  g_task_return_boolean (task, TRUE);
}

static void
gb_view_stack_actions_move_left (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  GbViewStack *self = user_data;
  GtkWidget *active_view;

  g_assert (GB_IS_VIEW_STACK (self));

  active_view = gb_view_stack_get_active_view (self);
  if (active_view == NULL || !GB_IS_VIEW (active_view))
    return;

  g_signal_emit_by_name (self, "split", active_view, GB_VIEW_GRID_MOVE_LEFT);
}

static void
gb_view_stack_actions_move_right (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbViewStack *self = user_data;
  GtkWidget *active_view;

  g_assert (GB_IS_VIEW_STACK (self));

  active_view = gb_view_stack_get_active_view (self);
  if (active_view == NULL || !GB_IS_VIEW (active_view))
    return;

  g_signal_emit_by_name (self, "split", active_view, GB_VIEW_GRID_MOVE_RIGHT);
}

static void
gb_view_stack_actions_save (GSimpleAction *action,
                            GVariant      *param,
                            gpointer       user_data)
{
  GbViewStack *self = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
}

static void
gb_view_stack_actions_save_as (GSimpleAction *action,
                               GVariant      *param,
                               gpointer       user_data)
{
  GbViewStack *self = user_data;

  g_assert (GB_IS_VIEW_STACK (self));
}

static void
gb_view_stack_actions_split_down (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbViewStack *self = user_data;
  GtkWidget *active_view;
  gboolean split_view;

  g_assert (GB_IS_VIEW_STACK (self));

  active_view = gb_view_stack_get_active_view (self);
  if (!GB_IS_VIEW (active_view))
    return;

  split_view = g_variant_get_boolean (param);
  gb_view_set_split_view (GB_VIEW (active_view), split_view);

  g_simple_action_set_state (action, param);
}

static void
gb_view_stack_actions_split_left (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbViewStack *self = user_data;
  GtkWidget *active_view;

  g_assert (GB_IS_VIEW_STACK (self));

  active_view = gb_view_stack_get_active_view (self);
  if (active_view == NULL || !GB_IS_VIEW (active_view))
    return;

  g_signal_emit_by_name (self, "split", active_view, GB_VIEW_GRID_SPLIT_LEFT);
}

static void
gb_view_stack_actions_split_right (GSimpleAction *action,
                                   GVariant      *param,
                                   gpointer       user_data)
{
  GbViewStack *self = user_data;
  GtkWidget *active_view;

  g_assert (GB_IS_VIEW_STACK (self));

  active_view = gb_view_stack_get_active_view (self);
  if (active_view == NULL || !GB_IS_VIEW (active_view))
    return;

  g_signal_emit_by_name (self, "split", active_view, GB_VIEW_GRID_SPLIT_RIGHT);
}

static const GActionEntry gGbViewStackActions[] = {
  { "close",       gb_view_stack_actions_close },
  { "move-left",   gb_view_stack_actions_move_left },
  { "move-right",  gb_view_stack_actions_move_right },
  { "save",        gb_view_stack_actions_save },
  { "save-as",     gb_view_stack_actions_save_as },
  { "split-down",  NULL, NULL, "false", gb_view_stack_actions_split_down },
  { "split-left",  gb_view_stack_actions_split_left },
  { "split-right", gb_view_stack_actions_split_right },
};

void
gb_view_stack_actions_init (GbViewStack *self)
{
  GSimpleActionGroup *actions;

  g_assert (GB_IS_VIEW_STACK (self));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), gGbViewStackActions,
                                   G_N_ELEMENTS (gGbViewStackActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "view-stack", G_ACTION_GROUP (actions));
}
