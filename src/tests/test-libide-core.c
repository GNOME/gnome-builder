/* test-libide-core.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-core.h>

#if 0
static void
dump_tree_foreach_cb (gpointer data,
                      gpointer user_data)
{
  guint *depth = user_data;
  g_autofree gchar *str = g_strnfill (*depth * 2, ' ');

  g_assert (IDE_IS_OBJECT (data));
  g_assert (depth != NULL);

  (*depth)++;
  g_printerr ("%s<%s at %p>\n", str, G_OBJECT_TYPE_NAME (data), data);
  ide_object_foreach (data, dump_tree_foreach_cb, depth);
  (*depth)--;
}

static void
dump_tree (IdeObject *root)
{
  guint depth = 1;
  g_printerr ("\n");
  g_printerr ("<%s at %p>\n", G_OBJECT_TYPE_NAME (root), root);
  ide_object_foreach (root, dump_tree_foreach_cb, &depth);
}
#endif

static void
test_ide_object_basic (void)
{
  IdeObject *root = ide_object_new (IDE_TYPE_OBJECT, NULL);
  IdeObject *child1 = ide_object_new (IDE_TYPE_OBJECT, root);
  IdeObject *child2 = ide_object_new (IDE_TYPE_OBJECT, root);
  IdeObject *child3 = ide_object_new (IDE_TYPE_OBJECT, root);
  IdeObject *toplevel = ide_object_ref_root (child3);
  GCancellable *cancel1 = ide_object_ref_cancellable (child1);

  g_object_add_weak_pointer (G_OBJECT (root), (gpointer *)&root);
  g_object_add_weak_pointer (G_OBJECT (child1), (gpointer *)&child1);
  g_object_add_weak_pointer (G_OBJECT (child2), (gpointer *)&child2);
  g_object_add_weak_pointer (G_OBJECT (child3), (gpointer *)&child3);

  g_assert (toplevel == root);
  g_object_unref (toplevel);

  g_object_unref (child1);
  g_object_unref (child2);
  g_object_unref (child3);

  g_assert_false (g_cancellable_is_cancelled (cancel1));

  g_assert_nonnull (root);
  g_assert_nonnull (child1);
  g_assert_nonnull (child2);
  g_assert_nonnull (child3);

  g_object_unref (root);

  g_assert_null (root);
  g_assert_null (child1);
  g_assert_null (child2);
  g_assert_null (child3);

  g_assert_true (g_cancellable_is_cancelled (cancel1));

  g_object_unref (cancel1);
}

static void
test_ide_object_readd (void)
{
  g_autoptr(IdeObject) a = ide_object_new (IDE_TYPE_OBJECT, NULL);
  g_autoptr(IdeObject) b = ide_object_new (IDE_TYPE_OBJECT, a);
  g_autoptr(IdeObject) p = ide_object_ref_parent (b);

  g_assert_nonnull (a);
  g_assert_nonnull (b);
  g_assert_nonnull (p);
  g_assert (a == p);

  g_clear_object (&p);

  ide_object_remove (a, b);
  p = ide_object_ref_parent (b);

  g_assert_nonnull (a);
  g_assert_nonnull (b);
  g_assert_null (p);

  g_clear_object (&p);

  ide_object_append (a, b);
  p = ide_object_ref_parent (b);

  g_assert_nonnull (a);
  g_assert_nonnull (b);
  g_assert_nonnull (p);
  g_assert (a == p);

  g_clear_object (&p);

  ide_object_destroy (a);
  p = ide_object_ref_parent (b);

  g_assert_nonnull (a);
  g_assert_nonnull (b);
  g_assert_null (p);
}

static void
destroyed_cb (IdeObject *object,
              guint     *location)
{
  g_assert (IDE_IS_OBJECT (object));
  (*location)--;
}

static void
test_ide_notification_basic (void)
{
  IdeObject *root = ide_object_new (IDE_TYPE_OBJECT, NULL);
  IdeNotifications *messages = ide_object_new (IDE_TYPE_NOTIFICATIONS, root);
  IdeNotification *message = ide_notification_new ();
  GIcon *icon = g_icon_new_for_string ("system-run-symbolic", NULL);
  g_autofree gchar *copy = NULL;
  gint clear1 = 1;
  gint clear2 = 1;
  gint clear3 = 1;

  ide_notifications_add_notification (messages, message);

  g_signal_connect (root, "destroy", G_CALLBACK (destroyed_cb), &clear1);
  g_signal_connect (messages, "destroy", G_CALLBACK (destroyed_cb), &clear2);
  g_signal_connect (message, "destroy", G_CALLBACK (destroyed_cb), &clear3);

  g_assert_cmpint (1, ==, G_OBJECT (root)->ref_count);
  g_assert_cmpint (2, ==, G_OBJECT (messages)->ref_count);
  g_assert_cmpint (2, ==, G_OBJECT (message)->ref_count);

  g_object_add_weak_pointer (G_OBJECT (root), (gpointer *)&root);
  g_object_add_weak_pointer (G_OBJECT (icon), (gpointer *)&icon);

  g_assert_true (ide_object_is_root (IDE_OBJECT (root)));
  g_assert_false (ide_object_is_root (IDE_OBJECT (messages)));
  g_assert_false (ide_object_is_root (IDE_OBJECT (message)));

  g_assert_null (ide_object_get_parent (root));
  g_assert (ide_object_get_parent (IDE_OBJECT (messages)) == (gpointer)root);
  g_assert (ide_object_get_parent (IDE_OBJECT (message)) == (gpointer)messages);

  g_assert_cmpint (1, ==, G_OBJECT (root)->ref_count);
  g_assert_cmpint (2, ==, G_OBJECT (messages)->ref_count);
  g_assert_cmpint (2, ==, G_OBJECT (message)->ref_count);

  ide_notification_set_title (message, "Foo");
  copy = ide_notification_dup_title (message);
  g_assert_cmpstr (copy, ==, "Foo");

  g_assert_cmpint (1, ==, G_OBJECT (icon)->ref_count);
  ide_notification_set_icon (message, icon);
  g_assert_cmpint (2, ==, G_OBJECT (icon)->ref_count);
  g_object_unref (icon);
  g_assert_nonnull (icon);
  g_assert_cmpint (1, ==, G_OBJECT (icon)->ref_count);

  g_assert_cmpint (1, ==, G_OBJECT (root)->ref_count);
  g_assert_cmpint (2, ==, G_OBJECT (messages)->ref_count);
  g_assert_cmpint (2, ==, G_OBJECT (message)->ref_count);

  g_object_unref (root);
  g_assert_null (root);

  g_assert_cmpint (1, ==, G_OBJECT (messages)->ref_count);
  g_assert_cmpint (1, ==, G_OBJECT (message)->ref_count);

  /* Make sure destruction propagated down the tree */
  g_assert (ide_object_get_parent (IDE_OBJECT (messages)) == NULL);
  g_assert (ide_object_get_parent (IDE_OBJECT (message)) == NULL);

  g_assert_true (ide_object_is_root (IDE_OBJECT (messages)));
  g_assert_true (ide_object_is_root (IDE_OBJECT (message)));

  g_assert_cmpint (clear1, ==, 0);
  g_assert_cmpint (clear2, ==, 0);
  g_assert_cmpint (clear3, ==, 0);

  g_object_add_weak_pointer (G_OBJECT (messages), (gpointer *)&messages);
  g_object_unref (messages);
  g_assert_null (messages);

  g_assert_cmpint (1, ==, G_OBJECT (message)->ref_count);
  g_assert (ide_object_get_parent (IDE_OBJECT (message)) == NULL);
  g_assert_true (ide_object_is_root (IDE_OBJECT (message)));

  g_object_add_weak_pointer (G_OBJECT (message), (gpointer *)&message);
  g_object_unref (message);
  g_assert_null (message);
  g_assert_null (icon);

  g_assert_cmpint (clear1, ==, 0);
  g_assert_cmpint (clear2, ==, 0);
  g_assert_cmpint (clear3, ==, 0);
}

static void
test_ide_notification_destroy (void)
{
  IdeObject *root = ide_object_new (IDE_TYPE_OBJECT, NULL);
  IdeNotifications *messages = ide_object_new (IDE_TYPE_NOTIFICATIONS, root);
  IdeNotification *message = ide_notification_new ();
  IdeObject *root_copy = root;
  gint clear1 = 1;
  gint clear2 = 1;
  gint clear3 = 1;

  ide_notifications_add_notification (messages, message);

  g_signal_connect (root, "destroy", G_CALLBACK (destroyed_cb), &clear1);
  g_signal_connect (messages, "destroy", G_CALLBACK (destroyed_cb), &clear2);
  g_signal_connect (message, "destroy", G_CALLBACK (destroyed_cb), &clear3);

  g_assert_cmpint (1, ==, G_OBJECT (root)->ref_count);
  g_assert_cmpint (2, ==, G_OBJECT (messages)->ref_count);
  g_assert_cmpint (2, ==, G_OBJECT (message)->ref_count);

  g_object_add_weak_pointer (G_OBJECT (root), (gpointer *)&root);
  g_object_add_weak_pointer (G_OBJECT (message), (gpointer *)&message);

  g_assert_cmpint (1, ==, ide_object_get_n_children (root));
  g_assert_cmpint (0, <, ide_object_get_n_children (IDE_OBJECT (messages)));

  g_object_unref (message);

  g_assert_cmpint (clear1, ==, 1);
  g_assert_cmpint (clear2, ==, 1);
  g_assert_cmpint (clear3, ==, 1);

  ide_object_destroy (root);

  g_assert_cmpint (0, ==, ide_object_get_n_children (root_copy));
  g_assert_cmpint (0, ==, ide_object_get_n_children (IDE_OBJECT (messages)));

  /* destroy should have caused this to dispose, thereby clearing
   * any weak pointers.
   */
  g_assert_null (message);

  g_object_add_weak_pointer (G_OBJECT (messages), (gpointer *)&messages);
  g_object_unref (messages);

  g_assert_cmpint (clear1, ==, 0);
  g_assert_cmpint (clear2, ==, 0);
  g_assert_cmpint (clear3, ==, 0);

  g_assert_null (root); /* weak cleared from dispose */
  g_assert_null (messages);
  g_assert_null (message);

  g_assert_cmpint (G_OBJECT (root_copy)->ref_count, ==, 1);
  g_object_add_weak_pointer (G_OBJECT (root_copy), (gpointer *)&root_copy);
  g_object_unref (root_copy);
  g_assert_null (root_copy);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/libide-core/IdeObject/basic", test_ide_object_basic);
  g_test_add_func ("/libide-core/IdeObject/re-add", test_ide_object_readd);
  g_test_add_func ("/libide-core/IdeNotification/basic", test_ide_notification_basic);
  g_test_add_func ("/libide-core/IdeNotification/destroy", test_ide_notification_destroy);
  return g_test_run ();
}
