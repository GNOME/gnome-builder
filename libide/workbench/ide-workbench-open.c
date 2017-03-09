/* ide-workbench-open.c
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

#define G_LOG_DOMAIN "ide-workbench-open"

#include <libpeas/peas.h>

#include "application/ide-application.h"
#include "util/ide-uri.h"
#include "workbench/ide-workbench-addin.h"
#include "workbench/ide-workbench-private.h"
#include "workbench/ide-workbench.h"

typedef struct
{
  IdeWorkbenchAddin *addin;
  gint               priority;
} IdeWorkbenchLoader;

typedef struct
{
  IdeWorkbench         *self;
  GTask                *task;
  IdeUri               *uri;
  GArray               *loaders;
  gchar                *content_type;
  IdeWorkbenchOpenFlags flags;
  gchar                *hint;
  guint                 did_collect : 1;
} IdeWorkbenchOpenUriState;

typedef struct
{
  gint          ref_count;
  IdeWorkbench *self;
  GTask        *task;
  GString      *error_msg;
} IdeWorkbenchOpenFilesState;

static void ide_workbench_open_uri_try_next (IdeWorkbenchOpenUriState *open_uri_state);

static void
ide_workbench_collect_loaders (PeasExtensionSet *set,
                               PeasPluginInfo   *plugin_info,
                               PeasExtension    *extension,
                               gpointer          user_data)
{
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)extension;
  IdeWorkbenchOpenUriState *open_uri_state = user_data;
  IdeWorkbenchLoader loader;
  gint priority = 0;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));

  if (ide_workbench_addin_can_open (addin,
                                    open_uri_state->uri,
                                    open_uri_state->content_type,
                                    &priority))
    {
      loader.addin = g_object_ref (addin);
      loader.priority = priority;
      g_array_append_val (open_uri_state->loaders, loader);
    }
}

static gint
ide_workbench_loader_compare (gconstpointer a,
                              gconstpointer b,
                              gpointer      user_data)
{
  const IdeWorkbenchLoader *loadera = a;
  const IdeWorkbenchLoader *loaderb = b;
  const gchar *hint = user_data;

  if (hint != NULL)
    {
      gboolean match;
      gchar *name;

      name = ide_workbench_addin_get_id (loadera->addin);
      match = g_strcmp0 (hint, name);
      g_free (name);
      if (match)
        return -1;

      name = ide_workbench_addin_get_id (loaderb->addin);
      match = g_strcmp0 (hint, name);
      g_free (name);
      if (match)
        return 1;
    }

  return loadera->priority - loaderb->priority;
}

static void
ide_workbench_loader_destroy (gpointer data)
{
  IdeWorkbenchLoader *loader = data;

  g_clear_object (&loader->addin);
  loader->priority = 0;
}

static void
ide_workbench_open_uri_state_free (gpointer data)
{
  IdeWorkbenchOpenUriState *open_uri_state = data;

  g_clear_pointer (&open_uri_state->loaders, g_array_unref);
  g_clear_pointer (&open_uri_state->uri, ide_uri_unref);
  g_clear_pointer (&open_uri_state->content_type, g_free);
  g_clear_pointer (&open_uri_state->hint, g_free);
  g_free (open_uri_state);
}

static void
ide_workbench_open_uri_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)object;
  IdeWorkbenchOpenUriState *open_uri_state = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (open_uri_state != NULL);

  if (ide_workbench_addin_open_finish (addin, result, &error))
    {
      g_task_return_boolean (open_uri_state->task, TRUE);
      g_object_unref (open_uri_state->task);
      return;
    }

  ide_workbench_open_uri_try_next (open_uri_state);
}

static void
ide_workbench_open_uri_try_next (IdeWorkbenchOpenUriState *open_uri_state)
{
  IdeWorkbenchLoader *loader;

  g_assert (open_uri_state != NULL);
  g_assert (G_IS_TASK (open_uri_state->task));
  g_assert (open_uri_state->loaders != NULL);
  g_assert (open_uri_state->uri != NULL);

  if (open_uri_state->did_collect == FALSE)
    {
      open_uri_state->did_collect = TRUE;
      peas_extension_set_foreach (open_uri_state->self->addins,
                                  ide_workbench_collect_loaders,
                                  open_uri_state);
      g_array_sort_with_data (open_uri_state->loaders,
                              ide_workbench_loader_compare,
                              open_uri_state->hint);
    }

  if (open_uri_state->loaders->len == 0)
    {
      gchar *uristr;

      uristr = ide_uri_to_string (open_uri_state->uri, IDE_URI_HIDE_AUTH_PARAMS);
      g_task_return_new_error (open_uri_state->task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No handler responded to \"%s\" with content-type \"%s\"",
                               uristr, open_uri_state->content_type ?: "");

      g_clear_object (&open_uri_state->task);
      g_free (uristr);

      return;
    }

  loader = &g_array_index (open_uri_state->loaders, IdeWorkbenchLoader, 0);

  ide_workbench_addin_open_async (loader->addin,
                                  open_uri_state->uri,
                                  open_uri_state->content_type,
                                  open_uri_state->flags,
                                  g_task_get_cancellable (open_uri_state->task),
                                  ide_workbench_open_uri_cb,
                                  open_uri_state);
}

static void
ide_workbench_open_discover_content_type_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  IdeWorkbenchOpenUriState *open_uri_state = user_data;
  GFileInfo *file_info;
  GFile *file = (GFile *)object;
  GError *error = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (open_uri_state != NULL);
  g_assert (G_IS_TASK (open_uri_state->task));

  file_info = g_file_query_info_finish (file, result, &error);

  if (file_info == NULL)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
  else
    {
      g_autofree gchar *name = NULL;
      const gchar *content_type;

      name = g_file_get_basename (file);
      content_type = g_file_info_get_content_type (file_info);

      /*
       * TODO: Make various overrides a bit more generic.
       *       It should support globs and such.
       */
      if ((g_strcmp0 (name, "Makefile.am") == 0) ||
          (g_strcmp0 (name, "GNUMakefile.am") == 0))
        content_type = "text/plain";

      open_uri_state->content_type = g_strdup (content_type);

      g_clear_object (&file_info);
    }

  ide_workbench_open_uri_try_next (open_uri_state);
}

static void
ide_workbench_open_discover_content_type (IdeWorkbenchOpenUriState *open_uri_state)
{
  g_autoptr(GFile) file = NULL;

  g_assert (open_uri_state != NULL);
  g_assert (G_IS_TASK (open_uri_state->task));
  g_assert (open_uri_state->loaders != NULL);
  g_assert (open_uri_state->uri != NULL);

  file = ide_uri_to_file (open_uri_state->uri);

  if (file != NULL)
    g_file_query_info_async (file,
                             G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                             G_FILE_QUERY_INFO_NONE,
                             G_PRIORITY_DEFAULT,
                             g_task_get_cancellable (open_uri_state->task),
                             ide_workbench_open_discover_content_type_cb,
                             open_uri_state);
  else
    ide_workbench_open_uri_try_next (open_uri_state);
}

void
ide_workbench_open_uri_async (IdeWorkbench         *self,
                              IdeUri               *uri,
                              const gchar          *hint,
                              IdeWorkbenchOpenFlags flags,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data)
{
  IdeWorkbenchOpenUriState *open_uri_state;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (uri != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  open_uri_state = g_new0 (IdeWorkbenchOpenUriState, 1);
  open_uri_state->self = self;
  open_uri_state->uri = ide_uri_ref (uri);
  open_uri_state->content_type = NULL;
  open_uri_state->loaders = g_array_new (FALSE, FALSE, sizeof (IdeWorkbenchLoader));
  open_uri_state->task = g_task_new (self, cancellable, callback, user_data);
  open_uri_state->hint = g_strdup (hint);
  open_uri_state->flags = flags;

  g_array_set_clear_func (open_uri_state->loaders,
                          ide_workbench_loader_destroy);

  g_task_set_task_data (open_uri_state->task,
                        open_uri_state,
                        ide_workbench_open_uri_state_free);

  ide_workbench_open_discover_content_type (open_uri_state);
}

gboolean
ide_workbench_open_uri_finish (IdeWorkbench  *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_workbench_open_files_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeWorkbench *self = (IdeWorkbench *)object;
  IdeWorkbenchOpenFilesState *open_files_state = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (open_files_state->self == self);
  g_assert (open_files_state->ref_count > 0);
  g_assert (open_files_state->error_msg != NULL);
  g_assert (G_IS_TASK (open_files_state->task));

  if (!ide_workbench_open_uri_finish (self, result, &error))
    {
      g_string_append (open_files_state->error_msg, error->message);
      g_clear_error (&error);
    }

  open_files_state->ref_count--;

  if (open_files_state->ref_count == 0)
    {
      if (open_files_state->error_msg->len > 0)
        g_task_return_new_error (open_files_state->task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "%s",
                                 open_files_state->error_msg->str);
      else
        g_task_return_boolean (open_files_state->task, TRUE);

      g_string_free (open_files_state->error_msg, TRUE);
      g_clear_object (&open_files_state->task);
      g_free (open_files_state);
    }
}

/**
 * ide_workbench_open_files_async:
 * @self: An #IdeWorkbench.
 * @files: An array of #GFile objects to be opened.
 * @n_files: The number of files given.
 * @hint: The id of an #IdeWorkbenchAddin that should be preferred as a loader.
 * @flags: A #IdeWorkbenchOpenFlags (if WORKBENCH_OPEN_FLAGS_BG is set, the buffer is loaded
 * but not made visible in the UI).
 * @cancellable: A #GCancellable.
 * @callback: A #GASyncReadyCallback.
 * @user_data: A #gpointer to hold user data.
 *
 * Starts the process of loading the buffers for the given @files, possibly
 * creating an #IdeEditorView for each depending on @flags.
 */
void
ide_workbench_open_files_async (IdeWorkbench         *self,
                                GFile               **files,
                                guint                 n_files,
                                const gchar          *hint,
                                IdeWorkbenchOpenFlags flags,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data)
{
  IdeWorkbenchOpenFilesState *open_files_state;
  gint i;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail ((n_files > 0 && files != NULL) || (n_files == 0));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (n_files == 0)
    {
      GTask *task;

      task = g_task_new (self, cancellable, callback, user_data);
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);

      return;
    }

  open_files_state = g_new0 (IdeWorkbenchOpenFilesState, 1);
  open_files_state->ref_count = n_files;
  open_files_state->self = self;
  open_files_state->task = g_task_new (self, cancellable, callback, user_data);
  open_files_state->error_msg = g_string_new (NULL);

  g_assert (n_files > 0);

  for (i = 0; i < n_files; i++)
    {
      IdeUri *uri;

      uri = ide_uri_new_from_file (files [i]);
      ide_workbench_open_uri_async (self,
                                    uri,
                                    hint,
                                    flags,
                                    cancellable,
                                    ide_workbench_open_files_cb,
                                    open_files_state);
      ide_uri_unref (uri);
    }
}

gboolean
ide_workbench_open_files_finish (IdeWorkbench  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_WORKBENCH (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
ide_workbench_open_project_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeContext) context = NULL;
  IdeWorkbench *workbench;
  guint32 present_time;
  GError *error = NULL;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  context = ide_context_new_finish (result, &error);

  if (context == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  workbench = g_task_get_source_object (task);

  if (workbench->context != NULL)
    {
      present_time = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (task), "GDK_CURRENT_TIME"));
      workbench = g_object_new (IDE_TYPE_WORKBENCH,
                                "application", IDE_APPLICATION_DEFAULT,
                                "disable-greeter", TRUE,
                                NULL);
      gtk_window_present_with_time  (GTK_WINDOW (workbench), present_time);
    }

  ide_workbench_set_context (workbench, context);

  g_task_return_boolean (task, TRUE);
}

void
ide_workbench_open_project_async (IdeWorkbench        *self,
                                  GFile               *file_or_directory,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (G_IS_FILE (file_or_directory));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  g_object_set_data (G_OBJECT (task),
                     "GDK_CURRENT_TIME",
                     GINT_TO_POINTER (GDK_CURRENT_TIME));

  ide_context_new_async (file_or_directory,
                         cancellable,
                         ide_workbench_open_project_cb,
                         g_object_ref (task));
}

gboolean
ide_workbench_open_project_finish (IdeWorkbench  *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
