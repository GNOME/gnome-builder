/* gbp-simple-similar-file-locator.c
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

#define G_LOG_DOMAIN "gbp-simple-similar-file-locator"

#include "config.h"

#include <libide-projects.h>
#include <libide-threading.h>
#include <libide-vcs.h>

#include "gbp-simple-similar-file-locator.h"

struct _GbpSimpleSimilarFileLocator
{
  IdeObject parent_instance;
};

static void
gbp_simple_similar_file_locator_list_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeVcs *vcs;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(ar = ide_g_file_find_finish (file, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (ar, g_object_unref);

  vcs = ide_task_get_task_data (task);
  store = g_list_store_new (G_TYPE_FILE);

  for (guint i = 0; i < ar->len; i++)
    {
      GFile *item = g_ptr_array_index (ar, i);

      if (!ide_vcs_is_ignored (vcs, item, NULL))
        g_list_store_append (store, item);
    }

  ide_task_return_object (task, g_steal_pointer (&store));

  IDE_EXIT;
}

static void
gbp_simple_similar_file_locator_list_async (IdeSimilarFileLocator *locator,
                                            GFile                 *file,
                                            GCancellable          *cancellable,
                                            GAsyncReadyCallback    callback,
                                            gpointer               user_data)
{
  GbpSimpleSimilarFileLocator *self = (GbpSimpleSimilarFileLocator *)locator;
  g_autofree char *name = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autofree char *pattern = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  char *ptr;

  IDE_ENTRY;

  g_assert (GBP_IS_SIMPLE_SIMILAR_FILE_LOCATOR (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_vcs_from_context (context);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_simple_similar_file_locator_list_async);
  ide_task_set_task_data (task, g_object_ref (vcs), g_object_unref);

  if (!(parent = g_file_get_parent (file)) ||
      !(name = g_file_get_basename (file)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Unexpected GFile");
      IDE_EXIT;
    }

  /* Strip off file suffix */
  if ((ptr = strrchr (name, '.')))
    *ptr = 0;

  /* remove -private or private suffix */
  if (g_str_has_suffix (name, "-private"))
    name[strlen (name) - strlen ("-private") - 1] = 0;

  /* Simple glob pattern */
  pattern = g_strdup_printf ("%s*", name);

  ide_g_file_find_with_depth_async (parent,
                                    pattern,
                                    2,
                                    cancellable,
                                    gbp_simple_similar_file_locator_list_cb,
                                    g_steal_pointer (&task));

  IDE_EXIT;
}

static GListModel *
gbp_simple_similar_file_locator_list_finish (IdeSimilarFileLocator  *locator,
                                             GAsyncResult           *result,
                                             GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_SIMPLE_SIMILAR_FILE_LOCATOR (locator));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
similar_file_locator_iface_init (IdeSimilarFileLocatorInterface *iface)
{
  iface->list_async = gbp_simple_similar_file_locator_list_async;
  iface->list_finish = gbp_simple_similar_file_locator_list_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSimpleSimilarFileLocator, gbp_simple_similar_file_locator, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SIMILAR_FILE_LOCATOR, similar_file_locator_iface_init))

static void
gbp_simple_similar_file_locator_class_init (GbpSimpleSimilarFileLocatorClass *klass)
{
}

static void
gbp_simple_similar_file_locator_init (GbpSimpleSimilarFileLocator *self)
{
}
