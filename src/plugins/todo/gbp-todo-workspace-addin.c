/* gbp-todo-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-todo-workspace-addin"

#include <glib/gi18n.h>

#include "gbp-todo-workspace-addin.h"
#include "gbp-todo-panel.h"

struct _GbpTodoWorkspaceAddin
{
  GObject       parent_instance;

  GbpTodoPanel *panel;
  GbpTodoModel *model;
  GCancellable *cancellable;
  GFile        *workdir;

  guint         has_presented : 1;
  guint         is_global_mining : 1;
};

static void
gbp_todo_workspace_addin_mine_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbpTodoModel *model = (GbpTodoModel *)object;
  g_autoptr(GbpTodoWorkspaceAddin) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_TODO_WORKSPACE_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_TODO_MODEL (model));

  /* We only do this once, safe to re-clear on per-file mining */
  self->is_global_mining = FALSE;

  if (!gbp_todo_model_mine_finish (model, result, &error))
    {
      if (!ide_error_ignore (error))
        g_warning ("%s", error->message);
    }

  if (self->panel != NULL)
    gbp_todo_panel_make_ready (self->panel);
}

static void
gbp_todo_workspace_addin_presented_cb (GbpTodoWorkspaceAddin *self,
                                       GbpTodoPanel          *panel)
{
  g_assert (GBP_IS_TODO_WORKSPACE_ADDIN (self));
  g_assert (GBP_IS_TODO_PANEL (panel));

  if (self->has_presented)
    return;

  self->has_presented = TRUE;
  self->is_global_mining = TRUE;

  gbp_todo_model_mine_async (self->model,
                             self->workdir,
                             self->cancellable,
                             gbp_todo_workspace_addin_mine_cb,
                             g_object_ref (self));
}

static void
gbp_todo_workspace_addin_buffer_saved (GbpTodoWorkspaceAddin *self,
                                       IdeBuffer             *buffer,
                                       IdeBufferManager      *bufmgr)
{
  GFile *file;

  g_assert (GBP_IS_TODO_WORKSPACE_ADDIN (self));
  g_assert (self->model != NULL);
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));

  if (!self->has_presented || self->is_global_mining)
    return;

  file = ide_buffer_get_file (buffer);
  gbp_todo_model_mine_async (self->model,
                             file,
                             self->cancellable,
                             gbp_todo_workspace_addin_mine_cb,
                             g_object_ref (self));
}

static void
gbp_todo_workspace_addin_load (IdeWorkspaceAddin *addin,
                               IdeWorkspace      *workspace)
{
  GbpTodoWorkspaceAddin *self = (GbpTodoWorkspaceAddin *)addin;
  g_autoptr(PanelPosition) position = NULL;
  IdeBufferManager *bufmgr;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (GBP_IS_TODO_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->cancellable = g_cancellable_new ();

  context = ide_workspace_get_context (workspace);
  vcs = ide_vcs_from_context (context);
  workdir = ide_vcs_get_workdir (vcs);
  bufmgr = ide_buffer_manager_from_context (context);

  self->workdir = g_object_ref (workdir);

  g_signal_connect_object (bufmgr,
                           "buffer-saved",
                           G_CALLBACK (gbp_todo_workspace_addin_buffer_saved),
                           self,
                           G_CONNECT_SWAPPED);

  self->model = gbp_todo_model_new (vcs);

  self->panel = g_object_new (GBP_TYPE_TODO_PANEL,
                              "title", _("TODO/FIXMEs"),
                              "icon-name", "builder-todo-symbolic",
                              "model", self->model,
                              NULL);
  g_signal_connect_object (self->panel,
                           "presented",
                           G_CALLBACK (gbp_todo_workspace_addin_presented_cb),
                           self,
                           G_CONNECT_SWAPPED);

  position = panel_position_new ();
  panel_position_set_area (position, PANEL_AREA_START);
  panel_position_set_row (position, 0);
  panel_position_set_depth (position, 3);

  ide_workspace_add_pane (workspace, IDE_PANE (self->panel), position);
}

static void
gbp_todo_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                 IdeWorkspace      *workspace)
{
  GbpTodoWorkspaceAddin *self = (GbpTodoWorkspaceAddin *)addin;
  IdeBufferManager *bufmgr;
  IdeContext *context;

  g_assert (GBP_IS_TODO_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  context = ide_widget_get_context (GTK_WIDGET (workspace));
  bufmgr = ide_buffer_manager_from_context (context);

  g_signal_handlers_disconnect_by_func (bufmgr,
                                        G_CALLBACK (gbp_todo_workspace_addin_buffer_saved),
                                        self);

  panel_widget_close (PANEL_WIDGET (self->panel));
  self->panel = NULL;

  g_clear_object (&self->model);
  g_clear_object (&self->workdir);
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_todo_workspace_addin_load;
  iface->unload = gbp_todo_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTodoWorkspaceAddin, gbp_todo_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN,
                                                workspace_addin_iface_init))

static void
gbp_todo_workspace_addin_class_init (GbpTodoWorkspaceAddinClass *klass)
{
}

static void
gbp_todo_workspace_addin_init (GbpTodoWorkspaceAddin *self)
{
}
