/* ide-layout-grid-actions.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-layout-grid"

#include "config.h"

#include "layout/ide-layout-grid.h"
#include "layout/ide-layout-private.h"

static void
ide_layout_grid_actions_focus_neighbor (GSimpleAction *action,
                                        GVariant      *variant,
                                        gpointer       user_data)
{
  IdeLayoutGrid *self = user_data;
  GtkDirectionType dir;

  g_return_if_fail (G_IS_SIMPLE_ACTION (action));
  g_return_if_fail (variant != NULL);
  g_return_if_fail (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT32));
  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));

  dir = (GtkDirectionType)g_variant_get_int32 (variant);

  switch (dir)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_UP:
    case GTK_DIR_DOWN:
    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      ide_layout_grid_focus_neighbor (self, dir);
      break;

    default:
      g_return_if_reached ();
    }
}

static const GActionEntry actions[] = {
  { "focus-neighbor", ide_layout_grid_actions_focus_neighbor, "i" },
};

void
_ide_layout_grid_init_actions (IdeLayoutGrid *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "layoutgrid", G_ACTION_GROUP (group));
}
