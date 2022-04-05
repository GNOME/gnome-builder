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

static void gbp_vim_editor_page_addin_load                       (IdeEditorPageAddin    *addin,
                                                                  IdeEditorPage         *page);
static void gbp_vim_editor_page_addin_unload                     (IdeEditorPageAddin    *addin,
                                                                  IdeEditorPage         *page);
static void gbp_vim_editor_page_addin_notify_active_cb           (GbpVimEditorPageAddin *self,
                                                                  GParamSpec            *pspec,
                                                                  GbpVimWorkspaceAddin  *workspace_addin);
static void gbp_vim_editor_page_addin_notify_command_bar_text_cb (GbpVimEditorPageAddin *self,
                                                                  GParamSpec            *pspec,
                                                                  GtkSourceVimIMContext *im_context);
static void gbp_vim_editor_page_addin_notify_command_text_cb     (GbpVimEditorPageAddin *self,
                                                                  GParamSpec            *pspec,
                                                                  GtkSourceVimIMContext *im_context);
static void gbp_vim_editor_page_addin_update                     (GbpVimEditorPageAddin *self);

struct _GbpVimEditorPageAddin
{
  GObject               parent_instance;

  /* Unowned pointers */
  IdeEditorPage        *page;
  GbpVimWorkspaceAddin *workspace_addin;

  /* Owned references */
  GtkEventController   *key_controller;

  guint                 enabled : 1;
};

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

static void
gbp_vim_editor_page_addin_update (GbpVimEditorPageAddin *self)
{
  IdeWorkspaceAddin *workspace_addin;
  IdeSourceView *view;
  IdeWorkspace *workspace;
  GtkIMContext *im_context;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (self->page));

  view = ide_editor_page_get_view (self->page);
  im_context = gtk_event_controller_key_get_im_context (GTK_EVENT_CONTROLLER_KEY (self->key_controller));

  if (self->workspace_addin != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->workspace_addin,
                                            G_CALLBACK (gbp_vim_editor_page_addin_notify_active_cb),
                                            self);
      self->workspace_addin = NULL;
    }

  if (!(workspace = ide_widget_get_workspace (GTK_WIDGET (self->page))) ||
      !(workspace_addin = ide_workspace_addin_find_by_module_name (workspace, "vim")) ||
      !GBP_IS_VIM_WORKSPACE_ADDIN (workspace_addin))
    IDE_GOTO (disabled);

  self->workspace_addin = GBP_VIM_WORKSPACE_ADDIN (workspace_addin);

  g_signal_connect_object (self->workspace_addin,
                           "notify::active",
                           G_CALLBACK (gbp_vim_editor_page_addin_notify_active_cb),
                           self,
                           G_CONNECT_SWAPPED);

  if (!gbp_vim_workspace_addin_get_active (self->workspace_addin))
    IDE_GOTO (disabled);

  if (!self->enabled)
    {
      self->enabled = TRUE;
      gtk_im_context_set_client_widget (im_context, GTK_WIDGET (view));
      gtk_widget_add_controller (GTK_WIDGET (view), g_object_ref (self->key_controller));
    }

  IDE_EXIT;

disabled:
  if (self->enabled)
    {
      self->enabled = FALSE;
      gtk_im_context_set_client_widget (im_context, NULL);
      gtk_widget_remove_controller (GTK_WIDGET (view), self->key_controller);
      gtk_event_controller_reset (self->key_controller);
    }

  IDE_EXIT;
}

static void
gbp_vim_editor_page_addin_notify_active_cb (GbpVimEditorPageAddin *self,
                                            GParamSpec            *pspec,
                                            GbpVimWorkspaceAddin  *workspace_addin)
{
  g_assert (GBP_IS_VIM_EDITOR_PAGE_ADDIN (self));
  g_assert (GBP_IS_VIM_WORKSPACE_ADDIN (workspace_addin));

  gbp_vim_editor_page_addin_update (self);
}

static void
gbp_vim_editor_page_addin_notify_command_bar_text_cb (GbpVimEditorPageAddin *self,
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
gbp_vim_editor_page_addin_notify_command_text_cb (GbpVimEditorPageAddin *self,
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
  GtkIMContext *im_context;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PAGE (page));

  self->page = page;

  self->key_controller = gtk_event_controller_key_new ();
  im_context = gtk_source_vim_im_context_new ();
  gtk_event_controller_set_propagation_phase (self->key_controller, GTK_PHASE_CAPTURE);
  gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (self->key_controller), im_context);

  g_signal_connect_object (im_context,
                           "notify::command-bar-text",
                           G_CALLBACK (gbp_vim_editor_page_addin_notify_command_bar_text_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (im_context,
                           "notify::command-text",
                           G_CALLBACK (gbp_vim_editor_page_addin_notify_command_text_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_unref (im_context);

  gbp_vim_editor_page_addin_update (self);

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

  if (self->enabled)
    {
      IdeSourceView *view = ide_editor_page_get_view (page);

      self->enabled = FALSE;
      gtk_widget_remove_controller (GTK_WIDGET (view), self->key_controller);
    }

  g_clear_object (&self->key_controller);

  self->page = NULL;
  self->workspace_addin = NULL;

  IDE_EXIT;
}
