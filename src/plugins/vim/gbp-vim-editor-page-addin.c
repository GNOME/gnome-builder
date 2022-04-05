/* gbp-vim-editor-page-addin.c
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

#define G_LOG_DOMAIN "gbp-vim-editor-page-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-editor.h>

#include "gbp-vim-editor-page-addin.h"
#include "gbp-vim-workspace-addin.h"

struct _GbpVimEditorPageAddin
{
  GObject               parent_instance;
  IdeEditorPage        *page;
  GbpVimWorkspaceAddin *workspace_addin;
};

static void
gbp_vim_editor_page_addin_update (GbpVimEditorPageAddin *self)
{
  IdeWorkspaceAddin *workspace_addin;
  IdeWorkspace *workspace;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (self->page));

  self->workspace_addin = NULL;

  workspace = ide_widget_get_workspace (GTK_WIDGET (self->page));
  workspace_addin = ide_workspace_addin_find_by_module_name (workspace, "vim");

  if (workspace_addin == NULL || !GBP_IS_VIM_WORKSPACE_ADDIN (workspace_addin))
    IDE_GOTO (failure);

  self->workspace_addin = GBP_VIM_WORKSPACE_ADDIN (workspace_addin);

failure:
  IDE_EXIT;
}

static void
notify_command_bar_text_cb (GbpVimEditorPageAddin *self,
                            GParamSpec            *pspec,
                            GtkSourceVimIMContext *im_context)
{
  const char *command_bar;

  g_assert (GBP_IS_VIM_EDITOR_PAGE_ADDIN (self));
  g_assert (GTK_SOURCE_IS_VIM_IM_CONTEXT (im_context));

  command_bar = gtk_source_vim_im_context_get_command_bar_text (im_context);

  if (self->workspace_addin)
    gbp_vim_workspace_addin_set_command_bar (self->workspace_addin, command_bar);
}

static void
notify_command_text_cb (GbpVimEditorPageAddin *self,
                        GParamSpec            *pspec,
                        GtkSourceVimIMContext *im_context)
{
  const char *command;

  g_assert (GBP_IS_VIM_EDITOR_PAGE_ADDIN (self));
  g_assert (GTK_SOURCE_IS_VIM_IM_CONTEXT (im_context));

  command = gtk_source_vim_im_context_get_command_text (im_context);

  if (self->workspace_addin)
    gbp_vim_workspace_addin_set_command (self->workspace_addin, command);
}

static void
gbp_vim_editor_page_addin_load (IdeEditorPageAddin *addin,
                                IdeEditorPage      *page)
{
  GbpVimEditorPageAddin *self = (GbpVimEditorPageAddin *)addin;
  GtkEventController *key;
  IdeSourceView *view;
  GtkIMContext *im_context;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  self->page = page;

  gbp_vim_editor_page_addin_update (self);

  view = ide_editor_page_get_view (page);
  key = gtk_event_controller_key_new ();
  im_context = gtk_source_vim_im_context_new ();
  gtk_im_context_set_client_widget (im_context, GTK_WIDGET (view));
  gtk_event_controller_set_propagation_phase (key, GTK_PHASE_CAPTURE);
  gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (key), im_context);
  gtk_widget_add_controller (GTK_WIDGET (view), key);

  /* TODO: Make this toggleable via GSettings, and bind a property from
   * the workspace addin to enable this.
   */

  g_signal_connect_object (im_context,
                           "notify::command-bar-text",
                           G_CALLBACK (notify_command_bar_text_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (im_context,
                           "notify::command-text",
                           G_CALLBACK (notify_command_text_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_unref (im_context);

  IDE_EXIT;
}

static void
gbp_vim_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                  IdeEditorPage      *page)
{
  GbpVimEditorPageAddin *self = (GbpVimEditorPageAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  IDE_EXIT;
}

static void
editor_page_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_vim_editor_page_addin_load;
  iface->unload = gbp_vim_editor_page_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVimEditorPageAddin, gbp_vim_editor_page_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_page_addin_iface_init))

static void
gbp_vim_editor_page_addin_class_init (GbpVimEditorPageAddinClass *klass)
{
}

static void
gbp_vim_editor_page_addin_init (GbpVimEditorPageAddin *self)
{
}
