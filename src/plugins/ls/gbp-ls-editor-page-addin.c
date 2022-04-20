/* gbp-ls-editor-page-addin.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-ls-editor-page-addin"

#include "config.h"

#include <libide-gtk.h>
#include <libide-editor.h>

#include "gbp-ls-editor-page-addin.h"
#include "gbp-ls-page.h"

struct _GbpLsEditorPageAddin
{
  GObject parent_instance;
};

static void
open_directory_cb (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  IdeEditorPage *editor = user_data;
  g_autoptr(GFile) directory = NULL;
  GbpLsPage *page;
  IdeBuffer *buffer;
  IdeFrame *frame;
  GFile *file;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_PAGE (editor));

  buffer = ide_editor_page_get_buffer (editor);
  file = ide_buffer_get_file (buffer);
  directory = g_file_get_parent (file);

  frame = IDE_FRAME (gtk_widget_get_ancestor (GTK_WIDGET (editor), IDE_TYPE_FRAME));
  /* When used from an editor view right click menu, the folder browser acts like vim's directory
   * listing, where it closes once a file is choosen.
   */
  page = g_object_new (GBP_TYPE_LS_PAGE,
                       "close-on-activate", TRUE,
                       "visible", TRUE,
                       NULL);
  panel_frame_add (PANEL_FRAME (frame), PANEL_WIDGET (page));
  gbp_ls_page_set_directory (page, directory);
  panel_widget_raise (PANEL_WIDGET (page));
}

static void
open_in_files_cb (GSimpleAction *action,
                  GVariant      *param,
                  gpointer       user_data)
{
  IdeEditorPage *editor = user_data;
  IdeBuffer *buffer;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_PAGE (editor));

  buffer = ide_editor_page_get_buffer (editor);
  ide_file_manager_show (ide_buffer_get_file (buffer), NULL);
}

static void
gbp_ls_editor_page_addin_load (IdeEditorPageAddin *addin,
                               IdeEditorPage      *page)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  static const GActionEntry actions[] = {
    { "open-directory", open_directory_cb },
    { "open-in-files", open_in_files_cb },
  };

  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   page);

  gtk_widget_insert_action_group (GTK_WIDGET (page), "ls", G_ACTION_GROUP (group));
}

static void
gbp_ls_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                 IdeEditorPage      *page)
{
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  gtk_widget_insert_action_group (GTK_WIDGET (page), "ls", NULL);
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_ls_editor_page_addin_load;
  iface->unload = gbp_ls_editor_page_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpLsEditorPageAddin, gbp_ls_editor_page_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN,
                                                editor_page_addin_iface_init))

static void
gbp_ls_editor_page_addin_class_init (GbpLsEditorPageAddinClass *klass)
{
}

static void
gbp_ls_editor_page_addin_init (GbpLsEditorPageAddin *self)
{
}
