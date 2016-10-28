#include "egg-menu-manager.h"

gint
main (gint   argc,
      gchar *argv[])
{
  EggMenuManager *manager;
  GMenu *menu;
  GtkWidget *widget;
  GError *error = NULL;
  GMenu *top;

  gtk_init (&argc, &argv);

  manager = egg_menu_manager_new ();

  egg_menu_manager_add_filename (manager, "menus.ui", &error);
  g_assert_no_error (error);

  egg_menu_manager_add_filename (manager, "menus-exten-1.ui", &error);
  g_assert_no_error (error);

  egg_menu_manager_add_filename (manager, "menus-exten-2.ui", &error);
  g_assert_no_error (error);

  egg_menu_manager_add_filename (manager, "menus-exten-3.ui", &error);
  g_assert_no_error (error);

  egg_menu_manager_add_filename (manager, "menus-exten-4.ui", &error);
  g_assert_no_error (error);

  egg_menu_manager_add_filename (manager, "menus-exten-5.ui", &error);
  g_assert_no_error (error);

  top = g_menu_new ();

  menu = egg_menu_manager_get_menu_by_id (manager, "menu-1");
  g_menu_append_submenu (top, "menu-1", G_MENU_MODEL (menu));

  menu = egg_menu_manager_get_menu_by_id (manager, "menu-2");
  g_menu_append_submenu (top, "menu-2", G_MENU_MODEL (menu));

  menu = egg_menu_manager_get_menu_by_id (manager, "menu-3");
  g_menu_append_submenu (top, "menu-3", G_MENU_MODEL (menu));

  menu = egg_menu_manager_get_menu_by_id (manager, "menu-4");
  g_menu_append_submenu (top, "menu-4", G_MENU_MODEL (menu));

  widget = gtk_menu_new_from_model (G_MENU_MODEL (top));
  gtk_menu_popup_at_pointer (GTK_MENU (widget), NULL);

  gtk_main ();

  return 0;
}
