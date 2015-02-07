/* test-context.c
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

#include <ide.h>

typedef struct
{
  GMainLoop    *main_loop;
  IdeContext   *context;
  GCancellable *cancellable;
  GError       *error;
} test_new_async_state;

static void
test_new_async_cb1 (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  test_new_async_state *state = user_data;
  state->context = ide_context_new_finish (result, &state->error);
  g_main_loop_quit (state->main_loop);
}

static void
test_new_async (void)
{
  test_new_async_state state = { 0 };
  IdeBuildSystem *bs;
  IdeVcs *vcs;
  GFile *project_file;
  const gchar *root_build_dir;

  project_file = g_file_new_for_path (TEST_DATA_DIR"/project1/configure.ac");

  state.main_loop = g_main_loop_new (NULL, FALSE);
  state.cancellable = g_cancellable_new ();

  ide_context_new_async (project_file, state.cancellable,
                         test_new_async_cb1, &state);

  g_main_loop_run (state.main_loop);

  g_assert_no_error (state.error);
  g_assert (state.context);

  bs = ide_context_get_build_system (state.context);
  g_assert (IDE_IS_AUTOTOOLS_BUILD_SYSTEM (bs));

  vcs = ide_context_get_vcs (state.context);
  g_assert (IDE_IS_GIT_VCS (vcs));

  root_build_dir = ide_context_get_root_build_dir (state.context);
  g_assert (g_str_has_suffix (root_build_dir, "/libide/builds"));

  g_clear_object (&state.cancellable);
  g_clear_object (&state.context);
  g_clear_error (&state.error);
  g_main_loop_unref (state.main_loop);
  g_clear_object (&project_file);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/Context/new_async", test_new_async);
  return g_test_run ();
}
