/* ide-editor.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-editor.h"
#include "ide-editor-page.h"

typedef struct _Focus
{
  IdeWorkspace  *workspace;
  PanelPosition *position;
  IdeLocation   *location;
  IdeBuffer     *buffer;
  GFile         *file;
} Focus;

static Focus *
focus_new (IdeWorkspace  *workspace,
           PanelPosition *position,
           IdeBuffer     *buffer,
           IdeLocation   *location)
{
  IdeBufferManager *bufmgr;
  IdeContext *context;
  Focus *focus;
  GFile *file = NULL;

  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (position != NULL);
  g_assert (!buffer || IDE_IS_BUFFER (buffer));
  g_assert (!location || IDE_IS_LOCATION (location));
  g_assert (buffer != NULL || location != NULL);

  context = ide_workspace_get_context (workspace);
  bufmgr = ide_buffer_manager_from_context (context);

  g_assert (IDE_IS_CONTEXT (context));
  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));

  if (location != NULL)
    file = ide_location_get_file (location);
  else
    file = ide_buffer_get_file (buffer);

  g_assert (buffer != NULL || file != NULL);

  if (buffer == NULL)
    buffer = ide_buffer_manager_find_buffer (bufmgr, file);

  focus = g_atomic_rc_box_alloc0 (sizeof *focus);
  focus->position = g_object_ref (position);
  g_set_object (&focus->workspace, workspace);
  g_set_object (&focus->buffer, buffer);
  g_set_object (&focus->location, location);
  g_set_object (&focus->file, file);

  return focus;
}

static void
focus_finalize (gpointer data)
{
  Focus *focus = data;

  g_clear_object (&focus->workspace);
  g_clear_object (&focus->location);
  g_clear_object (&focus->buffer);
  g_clear_object (&focus->file);
}

static void
focus_free (Focus *focus)
{
  g_atomic_rc_box_release_full (focus, focus_finalize);
}

static void
focus_complete (Focus        *focus,
                const GError *error)
{
  IdeEditorPage *page = NULL;
  PanelFrame *frame;

  IDE_ENTRY;

  g_assert (focus != NULL);
  g_assert (G_IS_FILE (focus->file));
  g_assert (!focus->location || IDE_IS_LOCATION (focus->location));
  g_assert (!focus->buffer || IDE_IS_BUFFER (focus->buffer));
  g_assert (focus->buffer || error != NULL);
  g_assert (IDE_IS_WORKSPACE (focus->workspace));
  g_assert (focus->position != NULL);

  if (error != NULL)
    {
      IdeContext *context = ide_workspace_get_context (focus->workspace);
      ide_context_warning (context,
                           /* translators: %s is replaced with the error message */
                           _("Failed to open file: %s"),
                           error->message);
      focus_free (focus);
      IDE_EXIT;
    }

  frame = ide_workspace_get_frame_at_position (focus->workspace, focus->position);

  if (frame != NULL)
    {
      guint n_pages = panel_frame_get_n_pages (PANEL_FRAME (frame));

      for (guint i = 0; i < n_pages; i++)
        {
          PanelWidget *child = panel_frame_get_page (PANEL_FRAME (frame), i);

          if (IDE_IS_EDITOR_PAGE (child))
            {
              IdeBuffer *buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (child));

              if (buffer == focus->buffer)
                {
                  page = IDE_EDITOR_PAGE (child);
                  break;
                }
            }
        }
    }

  g_assert (!page || IDE_IS_EDITOR_PAGE (page));

  if (page == NULL)
    {
      page = IDE_EDITOR_PAGE (ide_editor_page_new (focus->buffer));
      ide_workspace_add_page (focus->workspace, IDE_PAGE (page), focus->position);
    }

  if (focus->location != NULL)
    {
      IdeSourceView *view = ide_editor_page_get_view (page);
      GtkTextIter iter;

      ide_buffer_get_iter_at_location (focus->buffer, &iter, focus->location);
      gtk_text_buffer_select_range (GTK_TEXT_BUFFER (focus->buffer), &iter, &iter);
      ide_source_view_scroll_to_insert (view, GTK_DIR_TAB_BACKWARD);
    }

  if (frame != NULL)
    panel_frame_set_visible_child (frame, PANEL_WIDGET (page));

  gtk_widget_grab_focus (GTK_WIDGET (page));

  focus_free (focus);

  IDE_EXIT;
}

static void
ide_editor_load_file_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;
  Focus *focus = user_data;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (focus != NULL);
  g_assert (IDE_IS_WORKSPACE (focus->workspace));
  g_assert (focus->position != NULL);
  g_assert (G_IS_FILE (focus->file));

  if ((buffer = ide_buffer_manager_load_file_finish (bufmgr, result, &error)))
    g_set_object (&focus->buffer, buffer);

  focus_complete (focus, error);
}

static void
do_focus (IdeWorkspace  *workspace,
          PanelPosition *position,
          IdeBuffer     *buffer,
          IdeLocation   *location)
{
  g_autoptr(PanelPosition) local_position = NULL;
  IdeBufferManager *bufmgr;
  IdeContext *context;
  Focus *focus;

  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (!buffer || IDE_IS_BUFFER (buffer));
  g_assert (!location || IDE_IS_LOCATION (location));
  g_assert (buffer != NULL || location != NULL);

  context = ide_workspace_get_context (workspace);
  bufmgr = ide_buffer_manager_from_context (context);

  g_assert (IDE_IS_CONTEXT (context));
  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));

  if (position == NULL)
    position = local_position = panel_position_new ();

  focus = focus_new (workspace, position, buffer, location);

  if (focus->buffer == NULL)
    ide_buffer_manager_load_file_async (bufmgr,
                                        focus->file,
                                        IDE_BUFFER_OPEN_FLAGS_NONE,
                                        NULL,
                                        ide_workspace_get_cancellable (workspace),
                                        ide_editor_load_file_cb,
                                        focus);
  else
    focus_complete (focus, NULL);
}

void
ide_editor_focus_location (IdeWorkspace  *workspace,
                           PanelPosition *position,
                           IdeLocation   *location)
{
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));
  g_return_if_fail (IDE_IS_LOCATION (location));

  do_focus (workspace, position, NULL, location);
}

void
ide_editor_focus_buffer (IdeWorkspace  *workspace,
                         PanelPosition *position,
                         IdeBuffer     *buffer)
{
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  do_focus (workspace, position, buffer, NULL);
}
