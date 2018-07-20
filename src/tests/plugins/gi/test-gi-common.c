/* test-gi-common.c
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "test-ide-gi-common"

#include "test-gi-common.h"

#include <ide.h>

#include "../../../plugins/gi/ide-gi-types.h"
#include "../../../plugins/gi/ide-gi-repository.h"
#include "../../../plugins/gi/ide-gi-objects.h"

static gsize setup_init = 0;
static IdeGiRepository *global_repository = NULL;

static void
current_version_changed_cb (IdeGiRepository *self,
                            IdeGiVersion    *version,
                            gpointer         user_data)
{
  g_autoptr(GTask) task = (GTask *)user_data;
  g_autoptr(IdeGiVersion) current_version = NULL;

  g_assert (IDE_IS_GI_REPOSITORY (self));
  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_GI_VERSION (version) || version == NULL);
  g_assert (IDE_IS_MAIN_THREAD ());

  current_version = ide_gi_repository_get_current_version (self);
  g_assert (version == current_version);

  g_task_return_pointer (task, self, g_object_unref);
}

static void
new_repository_async_cb1 (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr(IdeContext) context = NULL;
  g_autofree gchar *fake_gir_path = NULL;
  GTask *task = (GTask *)user_data;
  IdeGiRepository *repository;
  IdeBuildSystem *bs;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_MAIN_THREAD ());

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);

  bs = ide_context_get_build_system (context);
  g_assert_cmpstr (G_OBJECT_TYPE_NAME (bs), ==, "GbpMesonBuildSystem");

  repository = g_object_new (IDE_TYPE_GI_REPOSITORY,
                             "context", context,
                             "update-on-build", FALSE,
                             NULL);

  fake_gir_path = g_build_filename (TEST_DATA_DIR, "gi", NULL);
  ide_gi_repository_add_gir_search_path (repository, fake_gir_path);
  ide_gi_repository_set_update_on_build (repository, TRUE);

  g_signal_connect_object (repository,
                           "current-version-changed",
                           G_CALLBACK (current_version_changed_cb),
                           task, 0);
}

static void
new_repository_async (GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  g_autofree gchar *path = g_build_filename (TEST_DATA_DIR, "ide-gi-tests", "meson.build", NULL);
  g_autoptr(GFile) project_file = g_file_new_for_path (path);
  GTask *task = g_task_new (NULL, cancellable, callback, user_data);

  ide_context_new_async (project_file,
                         cancellable,
                         new_repository_async_cb1,
                         task);
}

static void
setup_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  GTask *task = (GTask *)user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  IDE_ENTRY;

  global_repository = (IdeGiRepository *)g_task_propagate_pointer (G_TASK (result), &error);
  g_assert (IDE_IS_GI_REPOSITORY (global_repository));

  g_once_init_leave (&setup_init, 1);
  g_task_return_pointer (task, global_repository, g_object_unref);

  IDE_EXIT;
}

IdeGiRepository *
test_gi_common_setup_finish (GAsyncResult  *result,
                             GError       **error)
{
  g_assert (G_IS_ASYNC_RESULT (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
test_gi_common_setup_async (GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GTask *task = g_task_new (NULL, cancellable, callback, user_data);

  IDE_ENTRY;

  if (g_once_init_enter (&setup_init))
    new_repository_async (cancellable, setup_cb, task);
  else
    g_task_return_pointer (task, global_repository, g_object_unref);

  IDE_EXIT;
}
