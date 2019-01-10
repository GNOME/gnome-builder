/* ide-grid-column-actions.c
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

#define G_LOG_DOMAIN "ide-grid-column-actions"

#include "config.h"

#include "ide-gui-private.h"

static void
ide_grid_column_actions_close (GSimpleAction *action,
                               GVariant      *variant,
                               gpointer       user_data)
{
  IdeGridColumn *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_GRID_COLUMN (self));

  _ide_grid_column_try_close (self);
}

static const GActionEntry grid_column_actions[] = {
  { "close", ide_grid_column_actions_close },
};

void
_ide_grid_column_update_actions (IdeGridColumn *self)
{
  GtkWidget *grid;
  gboolean can_close;

  g_assert (IDE_IS_GRID_COLUMN (self));

  grid = gtk_widget_get_parent (GTK_WIDGET (self));

  if (grid == NULL || !IDE_IS_GRID (grid))
    {
      g_warning ("Attempt to update actions in unowned grid column");
      return;
    }

  can_close = (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (grid)) > 1);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "gridcolumn", "close",
                             "enabled", can_close,
                             NULL);
}

void
_ide_grid_column_init_actions (IdeGridColumn *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_assert (IDE_IS_GRID_COLUMN (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                  grid_column_actions,
                                  G_N_ELEMENTS (grid_column_actions),
                                  self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "gridcolumn",
                                  G_ACTION_GROUP (group));
}
