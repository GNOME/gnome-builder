/* test-ide-builder.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include <glib.h>
#include <ide.h>

#include "application/ide-application-tests.h"

static void
get_build_targets_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  IdeBuilder *builder = (IdeBuilder *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) targets = NULL;

  g_assert (IDE_IS_BUILDER (builder));
  g_assert (G_IS_TASK (task));

  targets = ide_builder_get_build_targets_finish (builder, result, &error);
  g_assert_no_error (error);
  g_assert (targets != NULL);

  g_task_return_boolean (task, TRUE);
}

static void
get_build_flags_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  IdeBuilder *builder = (IdeBuilder *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) flags = NULL;
  GCancellable *cancellable;
  gboolean found = FALSE;

  g_assert (IDE_IS_BUILDER (builder));
  g_assert (G_IS_TASK (task));

  flags = ide_builder_get_build_flags_finish (builder, result, &error);
  g_assert_no_error (error);
  g_assert (flags != NULL);
  for (guint i = 0; flags[i]; i++)
    found |= g_str_equal (flags[i], "-D_THIS_IS_PROJECT1");
  g_assert_cmpint (found, ==, TRUE);

  /* Now try to get the build targets */

  cancellable = g_task_get_cancellable (task);

  ide_builder_get_build_targets_async (builder,
                                       cancellable,
                                       get_build_targets_cb,
                                       g_steal_pointer (&task));
}

static void
build_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  IdeBuilder *builder = (IdeBuilder *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;
  IdeContext *context;
  IdeProject *project;

  build_result = ide_builder_build_finish (builder, result, &error);
  g_assert_no_error (error);
  g_assert (build_result != NULL);
  g_assert (IDE_IS_BUILD_RESULT (build_result));

  cancellable = g_task_get_cancellable (task);

  context = ide_object_get_context (IDE_OBJECT (builder));

  project = ide_context_get_project (context);
  file = ide_project_get_file_for_path (project, "project1.c");

  /* Now try to get the cflags for a file and ensure cflag extraction works */

  ide_builder_get_build_flags_async (builder,
                                     file,
                                     cancellable,
                                     get_build_flags_cb,
                                     g_steal_pointer (&task));
}

static void
context_loaded_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(IdeConfiguration) config = NULL;
  IdeBuildSystem *build_system;
  GCancellable *cancellable;
  GError *error = NULL;
  IdeVcs *vcs;
  GFile *workdir;
  g_autofree gchar *name = NULL;

  cancellable = g_task_get_cancellable (task);

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  vcs = ide_context_get_vcs (context);
  g_assert_cmpstr ("IdeDirectoryVcs", ==, G_OBJECT_TYPE_NAME (vcs));

  workdir = ide_vcs_get_working_directory (vcs);
  name = g_file_get_basename (workdir);
  g_assert_cmpstr (name, ==, "project1");


  build_system = ide_context_get_build_system (context);
  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert_cmpstr ("IdeAutotoolsBuildSystem", ==, G_OBJECT_TYPE_NAME (build_system));

  config = g_object_new (IDE_TYPE_CONFIGURATION,
                         "id", "test-build",
                         "app-id", "org.gnome.Project1",
                         "context", context,
                         "runtime-id", "host",
                         "device-id", "local",
                         NULL);

  ide_configuration_set_dirty (config, FALSE);
  g_assert_cmpint (FALSE, ==, ide_configuration_get_dirty (config));

  builder = ide_build_system_get_builder (build_system, config, &error);
  g_assert_no_error (error);
  g_assert (builder != NULL);
  g_assert (IDE_IS_BUILDER (builder));
  g_assert_cmpstr ("IdeAutotoolsBuilder", ==, G_OBJECT_TYPE_NAME (builder));

  /* Do a "build" that will only do autogen/configure and no gmake */
  ide_builder_build_async (builder,
                           IDE_BUILDER_BUILD_FLAGS_NO_BUILD,
                           NULL,
                           cancellable,
                           build_cb,
                           g_steal_pointer (&task));
}

static void
test_build_system_autotools (GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr(GFile) project_file = NULL;
  g_autofree gchar *path = NULL;
  const gchar *srcdir = g_getenv ("G_TEST_SRCDIR");
  g_autoptr(GTask) task = NULL;

  task = g_task_new (NULL, cancellable, callback, user_data);

  path = g_build_filename (srcdir, "data", "project1", "configure.ac", NULL);
  project_file = g_file_new_for_path (path);

  ide_context_new_async (project_file,
                         cancellable,
                         context_loaded_cb,
                         g_object_ref (task));
}

static void
project2_get_build_flags_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeBuilder *builder = (IdeBuilder *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) flags = NULL;
  gboolean found = FALSE;

  g_assert (IDE_IS_BUILDER (builder));
  g_assert (G_IS_TASK (task));

  flags = ide_builder_get_build_flags_finish (builder, result, &error);
  g_assert_no_error (error);
  g_assert (flags != NULL);
  for (guint i = 0; flags[i]; i++)
    found |= g_str_equal (flags[i], "-D_THIS_IS_PROJECT2");
  g_assert_cmpint (found, ==, TRUE);

  g_task_return_boolean (task, TRUE);
}

static void
project2_build_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  IdeBuilder *builder = (IdeBuilder *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;
  IdeContext *context;
  IdeProject *project;

  build_result = ide_builder_build_finish (builder, result, &error);
  g_assert (error != NULL);
  g_assert (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED));
  g_assert (build_result == NULL);

  cancellable = g_task_get_cancellable (task);

  context = ide_object_get_context (IDE_OBJECT (builder));

  project = ide_context_get_project (context);
  file = ide_project_get_file_for_path (project, "project2.c");

  /* Now try to get the cflags for a file and ensure cflag extraction works */

  ide_builder_get_build_flags_async (builder,
                                     file,
                                     cancellable,
                                     project2_get_build_flags_cb,
                                     g_steal_pointer (&task));
}

static void
directory_context_loaded (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(IdeConfiguration) config = NULL;
  IdeBuildSystem *build_system;
  GCancellable *cancellable;
  GError *error = NULL;
  IdeVcs *vcs;
  GFile *workdir;
  g_autofree gchar *name = NULL;

  cancellable = g_task_get_cancellable (task);

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  vcs = ide_context_get_vcs (context);
  g_assert_cmpstr ("IdeDirectoryVcs", ==, G_OBJECT_TYPE_NAME (vcs));

  workdir = ide_vcs_get_working_directory (vcs);
  name = g_file_get_basename (workdir);
  g_assert_cmpstr (name, ==, "project2");


  build_system = ide_context_get_build_system (context);
  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert_cmpstr ("IdeDirectoryBuildSystem", ==, G_OBJECT_TYPE_NAME (build_system));

  config = g_object_new (IDE_TYPE_CONFIGURATION,
                         "id", "test-build",
                         "app-id", "org.gnome.Project2",
                         "context", context,
                         "runtime-id", "host",
                         "device-id", "local",
                         NULL);

  ide_configuration_setenv (config, "CFLAGS", "-D_THIS_IS_PROJECT2");

  ide_configuration_set_dirty (config, FALSE);
  g_assert_cmpint (FALSE, ==, ide_configuration_get_dirty (config));

  builder = ide_build_system_get_builder (build_system, config, &error);
  g_assert_no_error (error);
  g_assert (builder != NULL);
  g_assert (IDE_IS_BUILDER (builder));
  g_assert_cmpstr ("IdeSimpleBuilder", ==, G_OBJECT_TYPE_NAME (builder));

  ide_builder_build_async (builder,
                           IDE_BUILDER_BUILD_FLAGS_NONE,
                           NULL,
                           cancellable,
                           project2_build_cb,
                           g_steal_pointer (&task));
}

static void
test_build_system_directory (GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr(GFile) project_file = NULL;
  g_autofree gchar *path = NULL;
  const gchar *srcdir = g_getenv ("G_TEST_SRCDIR");
  g_autoptr(GTask) task = NULL;

  task = g_task_new (NULL, cancellable, callback, user_data);

  path = g_build_filename (srcdir, "data", "project2", NULL);
  project_file = g_file_new_for_path (path);

  ide_context_new_async (project_file,
                         cancellable,
                         directory_context_loaded,
                         g_object_ref (task));
}

gint
main (gint   argc,
      gchar *argv[])
{
  IdeApplication *app;
  gint ret;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new ();
  ide_application_add_test (app, "/Ide/BuildSystem/autotools", test_build_system_autotools, NULL);
  ide_application_add_test (app, "/Ide/BuildSystem/directory", test_build_system_directory, NULL);
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return ret;
}
