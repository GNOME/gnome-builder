/* ide-directory-vcs.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-directory-vcs.h"
#include "ide-project.h"
#include "ide-project-files.h"

typedef struct
{
  GFile *working_directory;
} IdeDirectoryVcsPrivate;

#define LOAD_MAX_FILES 5000

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeDirectoryVcs, ide_directory_vcs, IDE_TYPE_VCS, 0,
                        G_ADD_PRIVATE (IdeDirectoryVcs)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

static GFile *
ide_directory_vcs_get_working_directory (IdeVcs *vcs)
{
  IdeDirectoryVcs *self = (IdeDirectoryVcs *)vcs;
  IdeDirectoryVcsPrivate *priv = ide_directory_vcs_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DIRECTORY_VCS (vcs), NULL);

  return priv->working_directory;
}

static gboolean
ide_directory_vcs_is_ignored (IdeVcs  *vcs,
                              GFile   *file,
                              GError **error)
{
  g_autofree gchar *reversed = NULL;
  g_autofree gchar *name = NULL;

  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_FILE (file));

  name = g_file_get_basename (file);
  reversed = g_strreverse (name);

  /* check suffixes, in reverse */
  if ((reversed [0] == '~') ||
      (strncmp (reversed, "al.", 3) == 0) ||        /* .la */
      (strncmp (reversed, "ol.", 3) == 0) ||        /* .lo */
      (strncmp (reversed, "o.", 2) == 0) ||         /* .o */
      (strncmp (reversed, "pws.", 4) == 0) ||       /* .swp */
      (strncmp (reversed, "sped.", 5) == 0) ||      /* .deps */
      (strncmp (reversed, "sbil.", 5) == 0) ||      /* .libs */
      (strncmp (reversed, "cyp.", 4) == 0) ||       /* .pyc */
      (strncmp (reversed, "oyp.", 4) == 0) ||       /* .pyo */
      (strncmp (reversed, "omg.", 4) == 0) ||       /* .gmo */
      (strncmp (reversed, "tig.", 4) == 0) ||       /* .git */
      (strncmp (reversed, "rzb.", 4) == 0) ||       /* .bzr */
      (strncmp (reversed, "nvs.", 4) == 0) ||       /* .svn */
      (strncmp (reversed, "pmatsrid.", 9) == 0) ||  /* .dirstamp */
      (strncmp (reversed, "hcg.", 4) == 0))         /* .gch */
    return TRUE;

  return FALSE;
}

static void
ide_directory_vcs_dispose (GObject *object)
{
  IdeDirectoryVcs *self = (IdeDirectoryVcs *)object;
  IdeDirectoryVcsPrivate *priv = ide_directory_vcs_get_instance_private (self);

  g_clear_object (&priv->working_directory);

  G_OBJECT_CLASS (ide_directory_vcs_parent_class)->dispose (object);
}

static void
ide_directory_vcs_class_init (IdeDirectoryVcsClass *klass)
{
  IdeVcsClass *vcs_class = IDE_VCS_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  vcs_class->get_working_directory = ide_directory_vcs_get_working_directory;
  vcs_class->is_ignored = ide_directory_vcs_is_ignored;

  object_class->dispose = ide_directory_vcs_dispose;
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
  IdeDirectoryVcsPrivate *priv = ide_directory_vcs_get_instance_private (self);
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

  priv->working_directory = g_object_ref (directory);

  files = g_object_new (IDE_TYPE_PROJECT_FILES,
                        "context", context,
                        "parent", root,
                        NULL);
  ide_project_item_append (root, files);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);

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
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_directory_vcs_init_async;
  iface->init_finish = ide_directory_vcs_init_finish;
}
