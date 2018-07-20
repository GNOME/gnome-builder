/* test-gi-version.c
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

#define G_LOG_DOMAIN "test-ide-gi-version"

#include "test-gi-common.h"
#include "test-gi-utils.h"

#include <ide.h>

#include "application/ide-application-tests.h"

#include "../../../plugins/gnome-builder-plugins.h"

#include "../../../plugins/gi/ide-gi-types.h"
#include "../../../plugins/gi/ide-gi-objects.h"

#include "../../../plugins/gi/ide-gi-index.h"
#include "../../../plugins/gi/ide-gi-repository.h"
#include "../../../plugins/gi/ide-gi-repository-private.h"
#include "../../../plugins/gi/ide-gi-version.h"

#include <string.h>

static IdeGiRepository *global_repository = NULL;
static guint current_count = 0;
static gint current_remove_count = -1;

static void
test_version_removed_cb (IdeGiIndex *index,
                         guint       count,
                         gpointer    user_data)
{
  GTask *task = (GTask *)user_data;

  g_assert (IDE_IS_GI_INDEX (index));
  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_MAIN_THREAD ());

  IDE_ENTRY;

  if (count == 0)
    {
      g_assert (current_remove_count == -1);
      current_remove_count = 0;

      IDE_TRACE_MSG ("version 0 removed");
    }
  else if (count == 1)
    {
      g_assert (current_remove_count == 0);
      current_remove_count = 1;

      IDE_TRACE_MSG ("version 1 removed");

      g_task_return_boolean (task, TRUE);
    }
  else
    g_assert_not_reached ();

  IDE_EXIT;
}

static void
test_current_version_changed_cb (IdeGiRepository *self,
                                 IdeGiVersion    *version,
                                 gpointer         user_data)
{
  GTask *task = (GTask *)user_data;
  g_autofree gchar *gir_path = NULL;
  guint count;

  g_assert (IDE_IS_GI_REPOSITORY (self));
  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_GI_VERSION (version) || version == NULL);
  g_assert (IDE_IS_MAIN_THREAD ());

  IDE_ENTRY;

  count = ide_gi_version_get_count (version);
  if (count == 1)
    {
      g_assert (current_count == 0);
      current_count = 1;

      IDE_TRACE_MSG ("version 1 created");

      gir_path = g_build_filename (TEST_DATA_DIR, "gi", "v2", NULL);
      ide_gi_repository_add_gir_search_path (global_repository, gir_path);
      ide_gi_repository_queue_update (global_repository, NULL);
    }
  else if (count == 2)
    {
      g_assert (current_count == 1);

      IDE_TRACE_MSG ("version 2 created");
    }
  else
    g_assert_not_reached ();

  IDE_EXIT;
}

static void
test_version_lifetime_cb (gpointer      source_object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  GTask *task = user_data;
  g_autoptr(IdeGiIndex) index = NULL;
  g_autoptr(IdeGiNamespace) ns = NULL;
  g_autoptr(IdeGiVersion) version = NULL;
  g_autofree gchar *gir_path = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_MAIN_THREAD ());

  IDE_ENTRY;

  global_repository = test_gi_common_setup_finish (result, &error);
  g_assert (IDE_IS_GI_REPOSITORY (global_repository));

  version = ide_gi_repository_get_current_version (global_repository);
  g_assert (IDE_IS_GI_VERSION (version));

  ide_gi_repository_set_update_on_build (global_repository, FALSE);

  g_signal_connect_object (global_repository,
                           "current-version-changed",
                           G_CALLBACK (test_current_version_changed_cb),
                           task, 0);

  index  = _ide_gi_repository_get_current_index (global_repository);
  g_signal_connect_object (index,
                           "version-removed",
                           G_CALLBACK (test_version_removed_cb),
                           task, 0);

  ns = ide_gi_version_lookup_namespace (version, "Gtk", 3, 0);
  g_assert (ns != NULL);

  gir_path = g_build_filename (TEST_DATA_DIR, "gi", "v1", NULL);
  ide_gi_repository_add_gir_search_path (global_repository, gir_path);
  ide_gi_repository_queue_update (global_repository, NULL);

  IDE_EXIT;
}

static void
test_version_lifetime_async (GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GTask *task;

  IDE_ENTRY;

  task = g_task_new (NULL, cancellable, callback, user_data);
  test_gi_common_setup_async (cancellable, (GAsyncReadyCallback)test_version_lifetime_cb, task);

  IDE_EXIT;
}

gint
main (gint   argc,
      gchar *argv[])
{
  static const gchar *required_plugins[] = { "buildconfig",
                                             "meson-plugin",
                                             "autotools-plugin",
                                             "directory-plugin",
                                             NULL };
  g_autoptr (IdeApplication) app = NULL;
  gboolean ret;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new (IDE_APPLICATION_MODE_TESTS | G_APPLICATION_NON_UNIQUE);
  ide_application_add_test (app,
                            "/Gi/repository/check_version",
                            test_version_lifetime_async,
                            NULL,
                            required_plugins);

  gnome_builder_plugins_init ();
  ret = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref (global_repository);

  return ret;
}
