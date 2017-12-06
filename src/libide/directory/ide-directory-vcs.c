/* ide-directory-vcs.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-directory-vcs"

#include <glib/gi18n.h>

#include "ide-context.h"

#include "directory/ide-directory-vcs.h"
#include "projects/ide-project.h"

struct _IdeDirectoryVcs
{
  IdeObject  parent_instances;
  GFile     *working_directory;
};

#define LOAD_MAX_FILES 5000

static void async_initable_iface_init (GAsyncInitableIface *iface);
static void vcs_iface_init            (IdeVcsInterface     *iface);

G_DEFINE_TYPE_EXTENDED (IdeDirectoryVcs, ide_directory_vcs, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS, vcs_iface_init)
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

enum {
  PROP_0,
  N_PROPS,

  /* Override Properties */
  PROP_BRANCH_NAME,
  PROP_WORKING_DIRECTORY,
};

static gchar *
ide_directory_vcs_get_branch_name (IdeVcs *vcs)
{
  return g_strdup (_("unversioned"));
}

static GFile *
ide_directory_vcs_get_working_directory (IdeVcs *vcs)
{
  IdeDirectoryVcs *self = (IdeDirectoryVcs *)vcs;

  g_return_val_if_fail (IDE_IS_DIRECTORY_VCS (vcs), NULL);

  /* Note: This function is expected to be thread-safe for
   *       those holding a reference to @vcs. So
   *       @working_directory cannot be changed after creation
   *       and must be valid for the lifetime of @vcs.
   */

  return self->working_directory;
}

static gboolean
ide_directory_vcs_is_ignored (IdeVcs  *vcs,
                              GFile   *file,
                              GError **error)
{
  g_autofree gchar *reversed = NULL;

  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_FILE (file));

  reversed = g_strreverse (g_file_get_basename (file));

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

  g_clear_object (&self->working_directory);

  G_OBJECT_CLASS (ide_directory_vcs_parent_class)->dispose (object);
}

static void
ide_directory_vcs_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeDirectoryVcs *self = IDE_DIRECTORY_VCS (object);

  switch (prop_id)
    {
    case PROP_BRANCH_NAME:
      g_value_take_string (value, ide_directory_vcs_get_branch_name (IDE_VCS (self)));
      break;

    case PROP_WORKING_DIRECTORY:
      g_value_set_object (value, ide_directory_vcs_get_working_directory (IDE_VCS (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_directory_vcs_class_init (IdeDirectoryVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_directory_vcs_dispose;
  object_class->get_property = ide_directory_vcs_get_property;

  g_object_class_override_property (object_class, PROP_BRANCH_NAME, "branch-name");
  g_object_class_override_property (object_class, PROP_WORKING_DIRECTORY, "working-directory");
}

static void
ide_directory_vcs_init (IdeDirectoryVcs *self)
{
}

static void
ide_directory_vcs_init_worker (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  IdeDirectoryVcs *self = source_object;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GError) error = NULL;
  GFile *file = task_data;
  GFileType file_type;

  g_assert (IDE_IS_DIRECTORY_VCS (self));
  g_assert (G_IS_FILE (file));

  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 cancellable,
                                 &error);

  if (file_info == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  file_type = g_file_info_get_file_type (file_info);

  /*
   * Note: Working directory may only be setup creation time of
   *       the vcs. So we set it in our GAsyncInitable worker only.
   */

  if (file_type == G_FILE_TYPE_DIRECTORY)
    self->working_directory = g_object_ref (file);
  else
    self->working_directory = g_file_get_parent (file);

  g_task_return_boolean (task, TRUE);
}

static void
ide_directory_vcs_init_async (GAsyncInitable      *initable,
                              int                  io_priority,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  IdeDirectoryVcs *self = (IdeDirectoryVcs *)initable;
  g_autoptr(GTask) task = NULL;
  IdeContext *context;
  GFile *project_file;

  g_return_if_fail (IDE_IS_DIRECTORY_VCS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (initable));
  project_file = ide_context_get_project_file (context);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (project_file), g_object_unref);
  g_task_run_in_thread (task, ide_directory_vcs_init_worker);
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

static gint
ide_directory_vcs_get_priority (IdeVcs *vcs)
{
  return G_MAXINT;
}

static void
vcs_iface_init (IdeVcsInterface *iface)
{
  iface->get_working_directory = ide_directory_vcs_get_working_directory;
  iface->is_ignored = ide_directory_vcs_is_ignored;
  iface->get_priority = ide_directory_vcs_get_priority;
  iface->get_branch_name = ide_directory_vcs_get_branch_name;
}
