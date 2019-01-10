/* ide-editor-surface-actions.c
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

#define G_LOG_DOMAIN "ide-editor-surface-actions"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-editor-private.h"

static void
ide_editor_surface_actions_new_file (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeEditorSurface *self = user_data;
  IdeBufferManager *bufmgr;
  IdeContext *context;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_SURFACE (self));

  context = ide_widget_get_context (GTK_WIDGET (self));
  bufmgr = ide_buffer_manager_from_context (context);

  ide_buffer_manager_load_file_async (bufmgr,
                                      NULL,
                                      IDE_BUFFER_OPEN_FLAGS_NONE,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);
}

static void
ide_editor_surface_actions_open_file (GSimpleAction *action,
                                      GVariant      *variant,
                                      gpointer       user_data)
{
  IdeEditorSurface *self = user_data;
  GtkFileChooserNative *chooser;
  IdeWorkbench *workbench;
  GtkWidget *workspace;
  gint ret;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_SURFACE (self));

  if (!(workbench = ide_widget_get_workbench (GTK_WIDGET (self))))
    return;

  workspace = gtk_widget_get_toplevel (GTK_WIDGET (self));

  chooser = gtk_file_chooser_native_new (_("Open File"),
                                         GTK_WINDOW (workspace),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         _("Open"),
                                         _("Cancel"));
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), FALSE);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);

  ret = gtk_native_dialog_run (GTK_NATIVE_DIALOG (chooser));

  if (ret == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GPtrArray) ar = NULL;
      GSList *files;

      ar = g_ptr_array_new_with_free_func (g_object_unref);
      files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (chooser));
      for (const GSList *iter = files; iter; iter = iter->next)
        g_ptr_array_add (ar, iter->data);
      g_slist_free (files);

      if (ar->len > 0)
        ide_workbench_open_all_async (workbench,
                                      (GFile **)ar->pdata,
                                      ar->len,
                                      "editor",
                                      NULL, NULL, NULL);
    }

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (chooser));
}

static void
collect_pages (GtkWidget *widget,
               gpointer   user_data)
{
  IdePage *view = (IdePage *)widget;
  GPtrArray *views = user_data;

  g_assert (views != NULL);
  g_assert (IDE_IS_PAGE (view));

  g_ptr_array_add (views, g_object_ref (view));
}

static void
ide_editor_surface_actions_close_all (GSimpleAction *action,
                                      GVariant      *param,
                                      gpointer       user_data)
{
  IdeEditorSurface *self = user_data;
  g_autoptr(GPtrArray) views = NULL;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_SURFACE (self));

  /* First collect all the views and hold a reference to them
   * so that we do not need to worry about contains being destroyed
   * as we work through the list.
   */
  views = g_ptr_array_new_full (0, g_object_unref);
  ide_grid_foreach_page (self->grid, collect_pages, views);

  for (guint i = 0; i < views->len; i++)
    {
      IdePage *view = g_ptr_array_index (views, i);

      /* TODO: Should we allow suspending the close with
       *       agree_to_close_async()?
       */

      gtk_widget_destroy (GTK_WIDGET (view));
    }
}

static const GActionEntry editor_actions[] = {
  { "new-file", ide_editor_surface_actions_new_file },
  { "open-file", ide_editor_surface_actions_open_file },
  { "close-all", ide_editor_surface_actions_close_all },
};

void
_ide_editor_surface_init_actions (IdeEditorSurface *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   editor_actions,
                                   G_N_ELEMENTS (editor_actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "editor", G_ACTION_GROUP (group));
}
