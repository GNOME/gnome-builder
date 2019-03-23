/* test-git-client.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "test-git-client"

#include "config.h"

#include <libide-core.h>
#include <libide-vcs.h>
#include <stdlib.h>

#include "gbp-git-client.h"

static GMainLoop *main_loop;

static void
discover_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  GbpGitClient *client = (GbpGitClient *)object;
  g_autoptr(GFile) file = user_data;
  g_autoptr(GFile) meson_build = user_data;
  g_autoptr(GError) error = NULL;
  gboolean r;

  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_FILE (file));

  r = gbp_git_client_create_repo_finish (client, result, &error);
  g_assert_no_error (error);
  g_assert_true (r);

  meson_build = g_file_get_child (file, "meson.build");

  gbp_git_client_discover_async (self,
                                 meson_build,
                                 NULL,
                                 discover_cb,
                                 g_object_ref (file));

  g_main_loop_quit (main_loop);
}

static void
create_repo_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  GbpGitClient *client = (GbpGitClient *)object;
  g_autoptr(GFile) file = user_data;
  g_autoptr(GFile) meson_build = user_data;
  g_autoptr(GError) error = NULL;
  gboolean r;

  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_FILE (file));

  r = gbp_git_client_create_repo_finish (client, result, &error);
  g_assert_no_error (error);
  g_assert_true (r);

  meson_build = g_file_get_child (file, "meson.build");

  gbp_git_client_discover_async (self,
                                 meson_build,
                                 NULL,
                                 discover_cb,
                                 g_object_ref (file));
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) here = g_file_new_for_path (".");
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *tmpdir = g_strdup ("test-git-client-XXXXXX");
  GbpGitClient *client;

  g_mkdtemp (tmpdir);
  g_print ("Tmpdir: %s\n", tmpdir);
  file = g_file_new_for_path (tmpdir);

  main_loop = g_main_loop_new (NULL, FALSE);
  context = ide_context_new ();
  ide_context_set_workdir (context, here);
  client = gbp_git_client_from_context (context);

  gbp_git_client_create_repo_async (client,
                                    file,
                                    FALSE,
                                    NULL,
                                    create_repo_cb,
                                    g_object_ref (file));

  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;
}
