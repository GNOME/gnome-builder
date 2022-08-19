/* gbp-find-other-file-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-find-other-file-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-editor.h>
#include <libide-projects.h>

#include "gbp-find-other-file-browser.h"
#include "gbp-find-other-file-popover.h"
#include "gbp-find-other-file-workspace-addin.h"

struct _GbpFindOtherFileWorkspaceAddin
{
  GObject                  parent_instance;
  IdeWorkspace            *workspace;
  GtkMenuButton           *menu_button;
  GtkLabel                *label;
  GtkImage                *image;
  GbpFindOtherFileBrowser *browser;
  GbpFindOtherFilePopover *popover;
};

static void
find_other_file_action (GbpFindOtherFileWorkspaceAddin *self,
                        GVariant                       *param)
{
  g_assert (GBP_IS_FIND_OTHER_FILE_WORKSPACE_ADDIN (self));

  if (self->menu_button != NULL &&
      gtk_widget_get_visible (GTK_WIDGET (self->menu_button)))
    gtk_menu_button_popup (self->menu_button);
}

IDE_DEFINE_ACTION_GROUP (GbpFindOtherFileWorkspaceAddin, gbp_find_other_file_workspace_addin, {
  { "focus", find_other_file_action },
})

static void
gbp_find_other_file_workspace_addin_clear (GbpFindOtherFileWorkspaceAddin *self)
{
  g_assert (GBP_IS_FIND_OTHER_FILE_WORKSPACE_ADDIN (self));

  gtk_widget_hide (GTK_WIDGET (self->menu_button));
  gbp_find_other_file_popover_set_model (self->popover, NULL);
  gbp_find_other_file_browser_set_file (self->browser, NULL);
}

static GListModel *
join_models (GListModel *a,
             GListModel *b)
{
  GListStore *joined = g_list_store_new (G_TYPE_LIST_MODEL);

  g_assert (G_IS_LIST_MODEL (a));
  g_assert (G_IS_LIST_MODEL (b));

  g_list_store_append (joined, a);
  g_list_store_append (joined, b);

  return G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (joined)));
}

static void
gbp_find_other_file_workspace_addin_list_similar_cb (GObject      *object,
                                                     GAsyncResult *result,
                                                     gpointer      user_data)
{
  IdeProject *project = (IdeProject *)object;
  g_autoptr(GbpFindOtherFileWorkspaceAddin) self = user_data;
  g_autoptr(GListModel) model = NULL;
  g_autoptr(GListModel) joined = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_PROJECT (project));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_FIND_OTHER_FILE_WORKSPACE_ADDIN (self));

  /* Maybe we were disposed already */
  if (self->workspace == NULL)
    IDE_EXIT;

  if (!(model = ide_project_list_similar_finish (project, result, &error)))
    {
      if (!ide_error_ignore (error))
        g_warning ("%s", error->message);
      gbp_find_other_file_workspace_addin_clear (self);
      IDE_EXIT;
    }

  g_assert (GBP_IS_FIND_OTHER_FILE_BROWSER (self->browser));
  g_assert (G_IS_LIST_MODEL (model));

  joined = join_models (model, G_LIST_MODEL (self->browser));
  gbp_find_other_file_popover_set_model (self->popover, joined);
  gtk_widget_show (GTK_WIDGET (self->menu_button));

  IDE_EXIT;
}

static void
gbp_find_other_file_workspace_addin_page_changed (IdeWorkspaceAddin *addin,
                                                  IdePage           *page)
{
  GbpFindOtherFileWorkspaceAddin *self = (GbpFindOtherFileWorkspaceAddin *)addin;
  IdeProject *project;
  IdeContext *context;
  GFile *file;

  IDE_ENTRY;

  g_assert (GBP_IS_FIND_OTHER_FILE_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  gbp_find_other_file_workspace_addin_clear (self);

  if (!IDE_IS_EDITOR_PAGE (page))
    IDE_EXIT;

  context = ide_workspace_get_context (self->workspace);
  project = ide_project_from_context (context);
  file = ide_editor_page_get_file (IDE_EDITOR_PAGE (page));

  gbp_find_other_file_browser_set_file (self->browser, file);

  ide_project_list_similar_async (project,
                                  file,
                                  NULL,
                                  gbp_find_other_file_workspace_addin_list_similar_cb,
                                  g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_find_other_file_workspace_addin_load (IdeWorkspaceAddin *addin,
                                          IdeWorkspace      *workspace)
{
  GbpFindOtherFileWorkspaceAddin *self = (GbpFindOtherFileWorkspaceAddin *)addin;
  g_autoptr(GFile) workdir = NULL;
  PanelStatusbar *statusbar;
  IdeContext *context;
  GtkBox *box;

  IDE_ENTRY;

  g_assert (GBP_IS_FIND_OTHER_FILE_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;

  context = ide_workspace_get_context (workspace);
  workdir = ide_context_ref_workdir (context);

  self->browser = gbp_find_other_file_browser_new ();
  gbp_find_other_file_browser_set_root (self->browser, workdir);

  self->popover = g_object_new (GBP_TYPE_FIND_OTHER_FILE_POPOVER,
                                NULL);
  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 6,
                      NULL);
  self->image = g_object_new (GTK_TYPE_IMAGE,
                              "icon-name", "folder-symbolic",
                              "pixel-size", 16,
                              NULL);
  gtk_box_append (box, GTK_WIDGET (self->image));
#if 0
  self->label = g_object_new (GTK_TYPE_LABEL,
                              "ellipsize", PANGO_ELLIPSIZE_END,
                              NULL);
  gtk_box_append (box, GTK_WIDGET (self->label));
#endif
  self->menu_button = g_object_new (GTK_TYPE_MENU_BUTTON,
                                    "focus-on-click", FALSE,
                                    "popover", self->popover,
                                    "direction", GTK_ARROW_UP,
                                    "child", box,
                                    "visible", FALSE,
                                    "tooltip-text", _("Similar Files (Ctrl+Shift+O)"),
                                    NULL);

  statusbar = ide_workspace_get_statusbar (workspace);
  panel_statusbar_add_suffix (statusbar, 10000, GTK_WIDGET (self->menu_button));

  IDE_EXIT;
}

static void
gbp_find_other_file_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                            IdeWorkspace      *workspace)
{
  GbpFindOtherFileWorkspaceAddin *self = (GbpFindOtherFileWorkspaceAddin *)addin;
  PanelStatusbar *statusbar;

  IDE_ENTRY;

  g_assert (GBP_IS_FIND_OTHER_FILE_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  g_clear_object (&self->browser);

  statusbar = ide_workspace_get_statusbar (workspace);
  panel_statusbar_remove (statusbar, GTK_WIDGET (self->menu_button));
  self->menu_button = NULL;
  self->popover = NULL;
  self->label = NULL;
  self->image = NULL;

  self->workspace = NULL;

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_find_other_file_workspace_addin_load;
  iface->unload = gbp_find_other_file_workspace_addin_unload;
  iface->page_changed = gbp_find_other_file_workspace_addin_page_changed;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpFindOtherFileWorkspaceAddin, gbp_find_other_file_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_find_other_file_workspace_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_find_other_file_workspace_addin_class_init (GbpFindOtherFileWorkspaceAddinClass *klass)
{
}

static void
gbp_find_other_file_workspace_addin_init (GbpFindOtherFileWorkspaceAddin *self)
{
}
