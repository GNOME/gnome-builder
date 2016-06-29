#include <egg-radio-box.h>
#include <stdlib.h>

gint
main (gint argc,
      gchar *argv[])
{
  GtkWindow *window;
  EggRadioBox *box;

  gtk_init (&argc, &argv);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "title", "Test EggRadioBox",
                         NULL);
  box = g_object_new (EGG_TYPE_RADIO_BOX,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (box));

  egg_radio_box_add_item (box, "1", "One");
  egg_radio_box_add_item (box, "2", "Two");
  egg_radio_box_add_item (box, "3", "Three");
  egg_radio_box_add_item (box, "4", "Four");
  egg_radio_box_add_item (box, "5", "Five");
  egg_radio_box_add_item (box, "6", "Six");

  g_signal_connect (window, "delete-event", gtk_main_quit, NULL);
  gtk_window_present (window);

  gtk_main ();

  return EXIT_SUCCESS;
}
