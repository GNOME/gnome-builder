/* ide-script-manager.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <girepository.h>

#include "ide-script.h"
#include "ide-script-manager.h"

struct _IdeScriptManager
{
  IdeObject  parent_instance;
  gchar     *scripts_directory;
  GList     *scripts;
};

typedef struct
{
  gint ref_count;
} LoadState;

G_DEFINE_TYPE (IdeScriptManager, ide_script_manager, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SCRIPTS_DIRECTORY,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

const gchar *
ide_script_manager_get_scripts_directory (IdeScriptManager *self)
{
  g_return_val_if_fail (IDE_IS_SCRIPT_MANAGER (self), NULL);

  return self->scripts_directory;
}

static void
ide_script_manager_set_scripts_directory (IdeScriptManager *self,
                                          const gchar      *scripts_directory)
{
  g_return_if_fail (IDE_IS_SCRIPT_MANAGER (self));
  g_return_if_fail (!self->scripts_directory);

  self->scripts_directory = g_strdup (scripts_directory);
}

static void
ide_script_manager_finalize (GObject *object)
{
  IdeScriptManager *self = (IdeScriptManager *)object;

  g_clear_pointer (&self->scripts_directory, g_free);

  g_list_foreach (self->scripts, (GFunc)g_object_unref, NULL);
  g_list_free (self->scripts);
  self->scripts = NULL;

  G_OBJECT_CLASS (ide_script_manager_parent_class)->finalize (object);
}

static void
ide_script_manager_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeScriptManager *self = IDE_SCRIPT_MANAGER (object);

  switch (prop_id)
    {
    case PROP_SCRIPTS_DIRECTORY:
      g_value_set_string (value, ide_script_manager_get_scripts_directory (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_script_manager_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeScriptManager *self = IDE_SCRIPT_MANAGER (object);

  switch (prop_id)
    {
    case PROP_SCRIPTS_DIRECTORY:
      ide_script_manager_set_scripts_directory (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_script_manager_class_init (IdeScriptManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_script_manager_finalize;
  object_class->get_property = ide_script_manager_get_property;
  object_class->set_property = ide_script_manager_set_property;

  gParamSpecs [PROP_SCRIPTS_DIRECTORY] =
    g_param_spec_string ("scripts-directory",
                         _("Scripts Directory"),
                         _("The local path to the directory containing scripts."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SCRIPTS_DIRECTORY,
                                   gParamSpecs [PROP_SCRIPTS_DIRECTORY]);
}

static void
ide_script_manager_init (IdeScriptManager *self)
{
}

static gboolean
allow_file (const gchar *name)
{
  /* NOTE:
   *
   * Add your allowed suffix here if you are adding a new scripting language
   * (ie: Lua, etc)
   */
  return g_str_has_suffix (name, ".js") ||
         g_str_has_suffix (name, ".py");
}

static void
ide_script_manager_get_files_worker (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  IdeScriptManager *self = source_object;
  gchar *directory = task_data;
  const gchar *name;
  GPtrArray *ar;
  GError *error = NULL;
  GDir *dir;

  g_assert (IDE_IS_SCRIPT_MANAGER (self));
  g_assert (directory);

  dir = g_dir_open (directory, 0, &error);

  if (!dir)
    {
      g_task_return_error (task, error);
      return;
    }

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  while ((name = g_dir_read_name (dir)))
    {
      g_autofree gchar *path = NULL;
      g_autoptr(GFile) file = NULL;

      if (!allow_file (name))
        continue;

      path = g_build_filename (directory, name, NULL);
      file = g_file_new_for_path (path);

      g_ptr_array_add (ar, g_object_ref (file));
    }

  g_dir_close (dir);

  g_task_return_pointer (task, ar, (GDestroyNotify)g_ptr_array_unref);
}

static void
ide_script_manager_get_files_async (IdeScriptManager    *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_SCRIPT_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (self->scripts_directory), g_free);
  g_task_run_in_thread (task, ide_script_manager_get_files_worker);
}

static GPtrArray *
ide_script_manager_get_files_finish (IdeScriptManager  *self,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_SCRIPT_MANAGER (self));
  g_assert (G_IS_TASK (task));

  return g_task_propagate_pointer (task, error);
}

static void
ide_script_manager_new_script_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeScriptManager *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeObject) res = NULL;
  g_autoptr(GError) error = NULL;
  LoadState *state;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  state = g_task_get_task_data (task);

  res = ide_object_new_finish (result, &error);

  if (!res)
    g_warning ("%s", error->message);

  if (--state->ref_count == 0)
    g_task_return_boolean (task, TRUE);

  self->scripts = g_list_prepend (self->scripts, g_object_ref (res));
}

static void
ide_script_manager_load_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeScriptManager *self = (IdeScriptManager *)object;
  IdeContext *context;
  g_autoptr(GTask) task = user_data;
  LoadState *state;
  GPtrArray *ar;
  GError *error = NULL;
  gsize i;

  g_assert (IDE_IS_SCRIPT_MANAGER (self));
  g_assert (G_IS_TASK (task));

  context = ide_object_get_context (IDE_OBJECT (self));

  ar = ide_script_manager_get_files_finish (self, result, &error);

  if (!ar)
    {
      if (g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_task_return_boolean (task, TRUE);
      else
        g_task_return_error (task, error);
      return;
    }

  state = g_new0 (LoadState, 1);
  state->ref_count = ar->len;
  g_task_set_task_data (task, state, g_free);

  if (!ar->len)
    {
      g_task_return_boolean (task, TRUE);
      g_ptr_array_unref (ar);
      return;
    }

  for (i = 0; i < ar->len; i++)
    {
      GFile *file;

      file = g_ptr_array_index (ar, i);

      ide_object_new_async (IDE_SCRIPT_EXTENSION_POINT,
                            G_PRIORITY_DEFAULT,
                            g_task_get_cancellable (task),
                            ide_script_manager_new_script_cb,
                            g_object_ref (task),
                            "context", context,
                            "file", file,
                            NULL);
    }

  g_ptr_array_unref (ar);
}

void
ide_script_manager_load_async  (IdeScriptManager    *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  GIRepository *repository;
  GITypelib *typelib;
  GError *error = NULL;

  g_return_if_fail (IDE_IS_SCRIPT_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  repository = g_irepository_get_default ();
  typelib = g_irepository_require (repository, "Ide", NULL, 0, &error);

  if (!typelib)
    {
      g_task_return_error (task, error);
      return;
    }

  ide_script_manager_get_files_async (self,
                                      cancellable,
                                      ide_script_manager_load_cb,
                                      g_object_ref (task));
}

gboolean
ide_script_manager_load_finish (IdeScriptManager  *self,
                                GAsyncResult      *result,
                                GError           **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_SCRIPT_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}
