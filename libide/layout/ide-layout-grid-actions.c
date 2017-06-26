/* ide-layout-grid-actions.c
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

#define G_LOG_DOMAIN "ide-layout-grid-actions"

#include "ide-layout-private.h"

static void
ide_layout_grid_action_close_stack (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  IdeLayoutGrid *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_LAYOUT_GRID (self));

  /*
   * Clicking the close button should have caused the stack to become the
   * current stack, so we can rely on that.
   */

  _ide_layout_grid_close_current_stack (self);
}

static const GActionEntry grid_actions[] = {
  { "close-stack", ide_layout_grid_action_close_stack },
};

void
_ide_layout_grid_init_actions (IdeLayoutGrid *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_assert (IDE_IS_LAYOUT_GRID (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   grid_actions,
                                   G_N_ELEMENTS (grid_actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "layoutgrid", G_ACTION_GROUP (group));
}

void
_ide_layout_grid_update_actions (IdeLayoutGrid *self)
{
  gboolean enabled;

  g_assert (IDE_IS_LAYOUT_GRID (self));

  enabled = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self)) > 1;
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "layoutgrid", "close-stack",
                             "enabled", enabled,
                             NULL);
}
