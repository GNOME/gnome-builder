#include "gb-navigation-list.h"
#include "gb-navigation-item.h"

static void
test_navigation_list_basic (void)
{
  GbNavigationList *list;
  GbNavigationItem *item;
  guint i;

  list = g_object_new (GB_TYPE_NAVIGATION_LIST, NULL);

  g_assert_cmpint (0, ==, gb_navigation_list_get_depth (list));

  for (i = 0; i < 32; i++)
    {
      item = gb_navigation_item_new ("test item");
      gb_navigation_list_append (list, item);
      g_assert (item == gb_navigation_list_get_current_item (list));
      g_assert_cmpint (i + 1, ==, gb_navigation_list_get_depth (list));
      g_assert_cmpint (0, ==, gb_navigation_list_get_can_go_forward (list));
      g_assert_cmpint ((i>0), ==, gb_navigation_list_get_can_go_backward (list));
    }

  item = gb_navigation_item_new ("test item");
  gb_navigation_list_append (list, item);
  g_assert (item == gb_navigation_list_get_current_item (list));
  g_assert_cmpint (32, ==, gb_navigation_list_get_depth (list));

  for (i = 0; i < 31; i++)
    {
      g_assert_cmpint (TRUE, ==,  gb_navigation_list_get_can_go_backward (list));
      g_assert_cmpint ((i!=0), ==, gb_navigation_list_get_can_go_forward (list));
      gb_navigation_list_go_backward (list);
    }

  g_assert_cmpint (FALSE, ==, gb_navigation_list_get_can_go_backward (list));

  for (i = 0; i < 31; i++)
    {
      g_assert_cmpint (TRUE, ==, gb_navigation_list_get_can_go_forward (list));
      g_assert_cmpint ((i!=0), ==, gb_navigation_list_get_can_go_backward (list));
      gb_navigation_list_go_forward (list);
    }

  g_assert_cmpint (FALSE, ==, gb_navigation_list_get_can_go_forward (list));

  g_clear_object (&list);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/NavigationList/basic", test_navigation_list_basic);
  return g_test_run ();
}
