/* test-ide-back-forward-list.c
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

#include <glib.h>
#include <stdlib.h>
#include <ide.h>

#include "application/ide-application-tests.h"

typedef struct
{
  GMainLoop    *main_loop;
  IdeContext   *context;
  GCancellable *cancellable;
  GError       *error;
} test_state_t;

static IdeBackForwardItem *
parse_item (IdeContext  *context,
            const gchar *str)
{
  IdeBackForwardItem *ret;
  IdeUri *uri;
  GError *error = NULL;

  uri = ide_uri_new (str, 0, &error);
  g_assert_no_error (error);
  g_assert (uri != NULL);

  ret = g_object_new (IDE_TYPE_BACK_FORWARD_ITEM,
                      "context", context,
                      "uri", uri,
                      NULL);

  g_clear_pointer (&uri, ide_uri_unref);

  return ret;
}

static void
exercise1 (IdeBackForwardList *list)
{
  IdeContext *context;
  const gchar *items[] = {
    "file:///home/christian/Projects/gnome-builder/libide/template/ide-template-state.c#L120_43",
    "file:///home/christian/Projects/gnome-builder/libide/template/ide-template.h#L35_0",
    "file:///home/christian/Projects/gnome-builder/libide/template/ide-template-parser.h#L29_0",
    "file:///home/christian/Projects/%20spaces/foo#L30_1",
  };
  gsize i;

  context = ide_object_get_context (IDE_OBJECT (list));

  for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
      IdeBackForwardItem *item;

      item = parse_item (context, items [i]);
      ide_back_forward_list_push (list, item);
      g_assert (item == ide_back_forward_list_get_current_item (list));
      g_assert (!ide_back_forward_list_get_can_go_forward (list));
      if (i > 0)
        g_assert (ide_back_forward_list_get_can_go_backward (list));
      g_object_unref (item);
    }

  for (i = 0; i < G_N_ELEMENTS (items) - 1; i++)
    {
      g_assert (ide_back_forward_list_get_can_go_backward (list));
      ide_back_forward_list_go_backward (list);
    }

  g_assert (!ide_back_forward_list_get_can_go_backward (list));
  g_test_expect_message ("ide-back-forward-list", G_LOG_LEVEL_WARNING,
                         "Cannot go backward, no more items in queue.");
  ide_back_forward_list_go_backward (list);

  for (i = 0; i < G_N_ELEMENTS (items) - 1; i++)
    {
      g_assert (ide_back_forward_list_get_can_go_forward (list));
      ide_back_forward_list_go_forward (list);
    }

  g_assert (!ide_back_forward_list_get_can_go_forward (list));
  g_test_expect_message ("ide-back-forward-list", G_LOG_LEVEL_WARNING,
                         "Cannot go forward, no more items in queue.");
  ide_back_forward_list_go_forward (list);
}

static void
test_basic_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  IdeBackForwardList *list;
  IdeContext *context;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);

  list = ide_context_get_back_forward_list (context);
  exercise1 (list);

  g_task_return_boolean (task, TRUE);

  g_object_unref (context);
}

static void
test_basic (GCancellable        *cancellable,
            GAsyncReadyCallback  callback,
            gpointer             user_data)
{
  g_autofree gchar *path = NULL;
  GFile *project_file;
  GTask *task;

  path = g_build_filename (g_get_current_dir (), TEST_DATA_DIR, "project1", "configure.ac", NULL);
  project_file = g_file_new_for_path (path);
  task = g_task_new (NULL, cancellable, callback, user_data);
  ide_context_new_async (project_file, cancellable, test_basic_cb, task);
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
  ide_application_add_test (app, "/Ide/BackForwardList/basic", test_basic, NULL);
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return ret;
}
