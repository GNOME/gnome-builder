#include <gtk/gtk.h>
#include <stdlib.h>

#include "egg-slider.h"

static void
connect_button (GtkBuilder  *builder,
                const gchar *name,
                GCallback    callback)
{
  EggSlider *slider = EGG_SLIDER (gtk_builder_get_object (builder, "slider"));
  GtkButton *button = GTK_BUTTON (gtk_builder_get_object (builder, name));

  g_assert (slider != NULL);
  g_assert (button != NULL);

  g_signal_connect_swapped (button, "clicked", callback, slider);
}

static void set_bottom (EggSlider *slider) { egg_slider_set_position (slider, EGG_SLIDER_BOTTOM); }
static void set_top    (EggSlider *slider) { egg_slider_set_position (slider, EGG_SLIDER_TOP);    }
static void set_left   (EggSlider *slider) { egg_slider_set_position (slider, EGG_SLIDER_LEFT);   }
static void set_right  (EggSlider *slider) { egg_slider_set_position (slider, EGG_SLIDER_RIGHT);  }
static void set_none   (EggSlider *slider) { egg_slider_set_position (slider, EGG_SLIDER_NONE);   }

int
main (int   argc,
      char *argv[])
{
  GtkBuilder *builder;
  GObject *window;
  GError *error = NULL;

  gtk_init (&argc, &argv);

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, "test-egg-slider.ui", &error);
  g_assert_no_error (error);

  window = gtk_builder_get_object (builder, "window");
  g_assert (window != NULL);

  connect_button (builder, "up_button", G_CALLBACK (set_bottom));
  connect_button (builder, "down_button", G_CALLBACK (set_top));
  connect_button (builder, "end_button", G_CALLBACK (set_left));
  connect_button (builder, "start_button", G_CALLBACK (set_right));
  connect_button (builder, "none_button", G_CALLBACK (set_none));

  g_signal_connect (window, "delete-event", gtk_main_quit, NULL);

  gtk_window_present (GTK_WINDOW (window));

  gtk_main ();

  g_object_unref (builder);

  return EXIT_SUCCESS;
}
