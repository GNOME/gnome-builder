/* ide-directory-vcs.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-directory-vcs.h"
#include "ide-project.h"
#include "ide-project-files.h"
#include "tasks/ide-load-directory-task.h"

typedef struct
{
  gpointer dummy;
} IdeDirectoryVcsPrivate;

enum
{
  PROP_0,
  LAST_PROP
};

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeDirectoryVcs, ide_directory_vcs, IDE_TYPE_VCS, 0,
                        G_ADD_PRIVATE (IdeDirectoryVcs)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

#if 0
static GParamSpec *gParamSpecs [LAST_PROP];
#endif

static void
ide_directory_vcs_finalize (GObject *object)
{
#if 0
  IdeDirectoryVcs *self = (IdeDirectoryVcs *)object;
  IdeDirectoryVcsPrivate *priv = ide_directory_vcs_get_instance_private (self);
#endif

  G_OBJECT_CLASS (ide_directory_vcs_parent_class)->finalize (object);
}

static void
ide_directory_vcs_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
#if 0
  IdeDirectoryVcs *vcs = IDE_DIRECTORY_VCS (object);
#endif

  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ide_directory_vcs_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
#if 0
  IdeDirectoryVcs *vcs = IDE_DIRECTORY_VCS (object);
#endif

  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ide_directory_vcs_class_init (IdeDirectoryVcsClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = ide_directory_vcs_finalize;
  object_class->get_property = ide_directory_vcs_get_property;
  object_class->set_property = ide_directory_vcs_set_property;
}

static void
ide_directory_vcs_init (IdeDirectoryVcs *self)
{
}

static void
ide_directory_vcs_init_async (GAsyncInitable      *initable,
                              int                  io_priority,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  IdeDirectoryVcs *self = (IdeDirectoryVcs *)initable;
  IdeProjectItem *root;
  IdeProjectItem *files;
  IdeProject *project;
  IdeContext *context;
  GFile *directory;
  GTask *task;

  g_return_if_fail (IDE_IS_DIRECTORY_VCS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (initable));
  directory = ide_context_get_project_file (context);
  project = ide_context_get_project (context);
  root = ide_project_get_root (project);

  files = g_object_new (IDE_TYPE_PROJECT_FILES,
                        "context", context,
                        "parent", root,
                        NULL);
  ide_project_item_append (root, files);

  task = ide_load_directory_task_new (self,
                                      directory,
                                      files,
                                      io_priority,
                                      cancellable,
                                      callback,
                                      user_data);

  g_object_unref (files);
  g_object_unref (task);
}

static gboolean
ide_directory_vcs_init_finish (GAsyncInitable  *initable,
                               GAsyncResult    *result,
                               GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_DIRECTORY_VCS (initable), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_boolean (task, error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_directory_vcs_init_async;
  iface->init_finish = ide_directory_vcs_init_finish;
}
