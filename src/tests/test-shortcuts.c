/* test-shortcuts.c
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

#include "config.h"

#include <girepository/girepository.h>

#include <libide-gui.h>

#include "ide-shortcut-bundle-private.h"

typedef struct
{
  guint open;
} TestParseBundle;

static void
test_parse_bundle_open (GSimpleAction *action,
                        GVariant      *param,
                        gpointer       data)
{
  TestParseBundle *state = data;
  state->open++;
}

static const GActionEntry actions[] = {
  { "open", test_parse_bundle_open },
};

static void
test_parse_bundle (void)
{
  GSimpleActionGroup *group;
  IdeShortcutBundle *bundle;
  GtkShortcut *shortcut;
  GtkWidget *widget;
  GError *error = NULL;
  GFile *file;
  gboolean r;
  guint pos = 0;
  TestParseBundle state = {0};

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   &state);

  widget = gtk_window_new ();
  gtk_widget_insert_action_group (widget, "test", G_ACTION_GROUP (group));

  bundle = ide_shortcut_bundle_new ();
  g_assert_nonnull (bundle);

  file = g_file_new_build_filename (g_getenv ("G_TEST_SRCDIR"), "test-shortcuts.json", NULL);
  g_assert_nonnull (file);
  g_assert_true (g_file_query_exists (file, NULL));

  r = ide_shortcut_bundle_parse (bundle, file, &error);
  g_assert_no_error (error);
  g_assert_true (r);

  g_assert_true (G_IS_LIST_MODEL (bundle));
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (bundle)) == GTK_TYPE_SHORTCUT);
  g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (bundle)), ==, 2);

  shortcut = g_list_model_get_item (G_LIST_MODEL (bundle), pos++);
  g_assert_nonnull (shortcut);
  g_assert_true (GTK_IS_SHORTCUT (shortcut));
  g_assert_false (gtk_shortcut_action_activate (gtk_shortcut_get_action (shortcut), 0, widget, NULL));
  g_assert_cmpint (state.open, ==, 0);
  g_object_unref (shortcut);

  shortcut = g_list_model_get_item (G_LIST_MODEL (bundle), pos++);
  g_assert_nonnull (shortcut);
  g_assert_true (GTK_IS_SHORTCUT (shortcut));
  g_assert_true (gtk_shortcut_action_activate (gtk_shortcut_get_action (shortcut), 0, widget, NULL));
  g_assert_cmpint (state.open, ==, 1);
  g_object_unref (shortcut);

  shortcut = g_list_model_get_item (G_LIST_MODEL (bundle), pos++);
  g_assert_null (shortcut);

  gtk_window_destroy (GTK_WINDOW (widget));
  g_assert_finalize_object (group);
  g_assert_finalize_object (bundle);
  g_assert_finalize_object (file);
}

int
main (int   argc,
      char *argv[])
{
  g_autofree char *path = NULL;
  g_autoptr(GError) error = NULL;

  g_assert_nonnull (g_getenv ("G_TEST_SRCDIR"));
  g_assert_nonnull (g_getenv ("G_TEST_BUILDDIR"));

  path = g_build_filename (g_getenv ("G_TEST_BUILDDIR"), "../", NULL);
  gi_repository_prepend_search_path (ide_get_gir_repository (), path);
  gi_repository_require (ide_get_gir_repository (), "Ide", PACKAGE_ABI_S, 0, &error);
  g_assert_no_error (error);

  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Ide/ShortcutBundle/parse", test_parse_bundle);

  return g_test_run ();
}
