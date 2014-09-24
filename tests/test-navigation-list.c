#include "gb-navigation-list.h"
#include "gb-navigation-item.h"

static void
test_navigation_list_depth (void)
{
  GbNavigationList *list;
  guint i;
  
  list = gb_navigation_list_new ();
  
  g_assert_cmpint (0, ==, gb_navigation_list_get_depth (list));
  
  for (i = 0; i < 32; i++)
    {
      GbNavigationItem *item;
      
      item = gb_navigation_item_new ("test item");
      gb_navigation_list_append (list, item);
      g_assert (item == gb_navigation_list_get_current_item (list));
      g_assert_cmpint (i + 1, ==, gb_navigation_list_get_depth (list));
    }
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/NavigationList/depth", test_navigation_list_depth);
  return g_test_run ();
}
