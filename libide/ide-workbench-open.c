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

#include <libpeas/peas.h>

#include "ide-uri.h"
#include "ide-workbench.h"
#include "ide-workbench-addin.h"
#include "ide-workbench-private.h"

typedef struct
{
  IdeWorkbenchAddin *addin;
  gint               priority;
} IdeWorkbenchLoader;

typedef struct
{
  GTask  *task;
  IdeUri *uri;
  GArray *loaders;
  gchar  *content_type;
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
                              gconstpointer b)
{
  const IdeWorkbenchLoader *loadera = a;
  const IdeWorkbenchLoader *loaderb = b;

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

  if (open_uri_state->loaders->len == 0)
    {
      gchar *uristr;

      uristr = ide_uri_to_string (open_uri_state->uri, IDE_URI_HIDE_AUTH_PARAMS);
      g_task_return_new_error (open_uri_state->task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No handler responded to %s",
                               uristr);

      g_clear_object (&open_uri_state->task);
      g_free (uristr);

      return;
    }

  loader = &g_array_index (open_uri_state->loaders, IdeWorkbenchLoader, 0);

  ide_workbench_addin_open_async (loader->addin,
                                  open_uri_state->uri,
                                  open_uri_state->content_type,
                                  g_task_get_cancellable (open_uri_state->task),
                                  ide_workbench_open_uri_cb,
                                  open_uri_state);
}

void
ide_workbench_open_uri_async (IdeWorkbench        *self,
                              IdeUri              *uri,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  IdeWorkbenchOpenUriState *open_uri_state;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (uri != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  open_uri_state = g_new0 (IdeWorkbenchOpenUriState, 1);
  open_uri_state->uri = ide_uri_ref (uri);
  open_uri_state->loaders = g_array_new (FALSE, FALSE, sizeof (IdeWorkbenchLoader));
  open_uri_state->task = g_task_new (self, cancellable, callback, user_data);
  g_array_set_clear_func (open_uri_state->loaders, ide_workbench_loader_destroy);
  peas_extension_set_foreach (self->addins, ide_workbench_collect_loaders, open_uri_state);
  g_array_sort (open_uri_state->loaders, ide_workbench_loader_compare);
  g_task_set_task_data (open_uri_state->task, open_uri_state, ide_workbench_open_uri_state_free);

  ide_workbench_open_uri_try_next (open_uri_state);
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

void
ide_workbench_open_files_async (IdeWorkbench         *self,
                                GFile               **files,
                                guint                 n_files,
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
