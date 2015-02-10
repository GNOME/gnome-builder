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
#include <ide.h>

typedef struct
{
  GMainLoop    *main_loop;
  IdeContext   *context;
  GCancellable *cancellable;
  GError       *error;
} test_state_t;

static IdeBackForwardItem *
parse_item (test_state_t *state,
            const gchar  *str)
{
  IdeBackForwardItem *ret;
  IdeSourceLocation *location;
  IdeFile *file;
  gchar **parts;
  GFile *gfile;
  gsize i;
  guint offset;
  guint line;
  guint line_offset;

  parts = g_strsplit (str, " ", 0);

  if (g_strv_length (parts) != 4)
    {
      g_strfreev (parts);
      return NULL;
    }

  gfile = g_file_new_for_path (parts [0]);

  file = g_object_new (IDE_TYPE_FILE,
                       "context", state->context,
                       "file", gfile,
                       NULL);

  location = ide_source_location_new (file,
                                      atoi (parts[1]),
                                      atoi(parts[2]),
                                      atoi(parts[3]));

  ret = g_object_new (IDE_TYPE_BACK_FORWARD_ITEM,
                      "context", state->context,
                      "location", location,
                      NULL);

  g_clear_object (&gfile);
  g_clear_object (&file);
  g_strfreev (parts);

  return ret;
}

static void
exercise1 (test_state_t       *state,
           IdeBackForwardList *list)
{
  const gchar *items[] = {
    "test.c 10 10 100",
    "test.c 20 10 200",
    "test.h 0 0 0",
    "test.c 1 2 3",
    "test.h 1 2 3",
  };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
      IdeBackForwardItem *item;

      item = parse_item (state, items [i]);
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
  test_state_t *state = user_data;

  state->context = ide_context_new_finish (result, &state->error);

  if (state->context)
    {
      IdeBackForwardList *list;

      list = ide_context_get_back_forward_list (state->context);
      exercise1 (state, list);
    }

  g_main_loop_quit (state->main_loop);
}

static void
test_basic (void)
{
  test_state_t state = { 0 };
  GFile *project_file;

  project_file = g_file_new_for_path (TEST_DATA_DIR"/project1/configure.ac");

  state.main_loop = g_main_loop_new (NULL, FALSE);
  state.cancellable = g_cancellable_new ();

  ide_context_new_async (project_file, state.cancellable,
                         test_basic_cb, &state);

  g_main_loop_run (state.main_loop);

  g_assert_no_error (state.error);
  g_assert (IDE_IS_CONTEXT (state.context));

  g_main_loop_unref (state.main_loop);
  g_clear_object (&state.cancellable);
  g_clear_object (&state.context);
  g_clear_object (&project_file);
  g_clear_error (&state.error);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/BackForwardList/basic", test_basic);
  return g_test_run ();
}
