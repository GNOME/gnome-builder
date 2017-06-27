/* ide-editor-perspective-actions.c
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

#define G_LOG_DOMAIN "ide-editor-perspective-actions"

#include "buffers/ide-buffer-manager.h"
#include "editor/ide-editor-private.h"
#include "util/ide-gtk.h"

static void
ide_editor_perspective_actions_new_document (GSimpleAction *action,
                                             GVariant      *variant,
                                             gpointer       user_data)
{
  IdeEditorPerspective *self = user_data;
  IdeWorkbench *workbench;
  IdeContext *context;
  IdeBufferManager *bufmgr;
  IdeBuffer *buffer;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  context = ide_workbench_get_context (workbench);
  bufmgr = ide_context_get_buffer_manager (context);
  buffer = ide_buffer_manager_create_temporary_buffer (bufmgr);

  g_clear_object (&buffer);
}

static const GActionEntry editor_actions[] = {
  { "new-document", ide_editor_perspective_actions_new_document },
};

void
_ide_editor_perspective_init_actions (IdeEditorPerspective *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   editor_actions,
                                   G_N_ELEMENTS (editor_actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "editor", G_ACTION_GROUP (group));
}
