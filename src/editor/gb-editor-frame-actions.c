/* gb-editor-frame-actions.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <gtksourceview/gtksource.h>

#include "gb-editor-frame-actions.h"
#include "gb-editor-frame-private.h"

static void
gb_editor_frame_actions_find (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  GbEditorFrame *self = user_data;

  g_assert (GB_IS_EDITOR_FRAME (self));

  gtk_revealer_set_reveal_child (self->search_revealer, TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}

static const GActionEntry GbEditorFrameActions[] = {
  { "find",     gb_editor_frame_actions_find },
};

void
gb_editor_frame_actions_init (GbEditorFrame *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_assert (GB_IS_EDITOR_FRAME (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), GbEditorFrameActions,
                                   G_N_ELEMENTS (GbEditorFrameActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "frame", G_ACTION_GROUP (group));
}
