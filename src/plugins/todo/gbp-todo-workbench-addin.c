/* gbp-todo-workbench-addin.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-todo-workbench-addin"

#include <glib/gi18n.h>

#include "gbp-todo-workbench-addin.h"
#include "gbp-todo-panel.h"

struct _GbpTodoWorkbenchAddin
{
  GObject       parent_instance;
  GbpTodoPanel *panel;
  GbpTodoModel *model;
  GCancellable *cancellable;
};

static void
gbp_todo_workbench_addin_mine_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbpTodoModel *model = (GbpTodoModel *)object;
  g_autoptr(GbpTodoWorkbenchAddin) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_TODO_WORKBENCH_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_TODO_MODEL (model));

  if (!gbp_todo_model_mine_finish (model, result, &error))
    ide_widget_warning (self->panel, "todo: %s", error->message);
}

static void
gbp_todo_workbench_addin_buffer_saved (GbpTodoWorkbenchAddin *self,
                                       IdeBuffer             *buffer,
                                       IdeBufferManager      *bufmgr)
{
  IdeFile *file;
  GFile *gfile;

  g_assert (GBP_IS_TODO_WORKBENCH_ADDIN (self));
  g_assert (self->model != NULL);
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));

  file = ide_buffer_get_file (buffer);
  gfile = ide_file_get_file (file);
  gbp_todo_model_mine_async (self->model,
                             gfile,
                             self->cancellable,
                             gbp_todo_workbench_addin_mine_cb,
                             g_object_ref (self));
}

static void
gbp_todo_workbench_addin_load (IdeWorkbenchAddin *addin,
                               IdeWorkbench      *workbench)
{
  GbpTodoWorkbenchAddin *self = (GbpTodoWorkbenchAddin *)addin;
  IdeEditorSidebar *sidebar;
  IdeBufferManager *bufmgr;
  IdePerspective *editor;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (GBP_IS_TODO_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->cancellable = g_cancellable_new ();

  context = ide_workbench_get_context (workbench);
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  bufmgr = ide_context_get_buffer_manager (context);
  editor = ide_workbench_get_perspective_by_name (workbench, "editor");
  sidebar = ide_editor_perspective_get_sidebar (IDE_EDITOR_PERSPECTIVE (editor));

  g_signal_connect_object (bufmgr,
                           "buffer-saved",
                           G_CALLBACK (gbp_todo_workbench_addin_buffer_saved),
                           self,
                           G_CONNECT_SWAPPED);

  self->model = gbp_todo_model_new (vcs);

  self->panel = g_object_new (GBP_TYPE_TODO_PANEL,
                              "model", self->model,
                              "visible", TRUE,
                              NULL);
  g_signal_connect (self->panel,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->panel);
  ide_editor_sidebar_add_section (sidebar,
                                  "todo",
                                  _("TODO/FIXMEs"),
                                  "emblem-ok-symbolic",
                                  NULL, NULL,
                                  GTK_WIDGET (self->panel),
                                  200);

  gbp_todo_model_mine_async (self->model,
                             workdir,
                             self->cancellable,
                             gbp_todo_workbench_addin_mine_cb,
                             g_object_ref (self));
}

static void
gbp_todo_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                 IdeWorkbench      *workbench)
{
  GbpTodoWorkbenchAddin *self = (GbpTodoWorkbenchAddin *)addin;
  IdeBufferManager *bufmgr;
  IdeContext *context;

  g_assert (GBP_IS_TODO_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  context = ide_workbench_get_context (workbench);
  bufmgr = ide_context_get_buffer_manager (context);

  g_signal_handlers_disconnect_by_func (bufmgr,
                                        G_CALLBACK (gbp_todo_workbench_addin_buffer_saved),
                                        self);

  gtk_widget_destroy (GTK_WIDGET (self->panel));

  g_clear_object (&self->model);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_todo_workbench_addin_load;
  iface->unload = gbp_todo_workbench_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpTodoWorkbenchAddin, gbp_todo_workbench_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                                workbench_addin_iface_init))

static void
gbp_todo_workbench_addin_class_init (GbpTodoWorkbenchAddinClass *klass)
{
}

static void
gbp_todo_workbench_addin_init (GbpTodoWorkbenchAddin *self)
{
}
