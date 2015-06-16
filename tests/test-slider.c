#include <gb-slider.h>

#define CSS_DATA \
  "GtkEntry { " \
  " border: none;" \
  " font-size: 1.2em;" \
  " border-radius: 0px;" \
  " color: #eeeeec;" \
  " background-image: linear-gradient(to bottom, #2e3436, #555753 10%);" \
  " box-shadow: inset 0px 3px 6px #2e3436;" \
  "}"

static GtkEntry *entry;
static GtkTextView *text_view;

static void
clicked_cb (GtkButton *button,
            GbSlider  *slider)
{
  GbSliderPosition position;

  g_assert (GTK_IS_BUTTON (button));
  g_assert (GB_IS_SLIDER (slider));

  position = gb_slider_get_position (slider) == GB_SLIDER_NONE ? GB_SLIDER_BOTTOM : GB_SLIDER_NONE;

  gb_slider_set_position (slider, position);

  gtk_widget_grab_focus (GTK_WIDGET (entry));
}

static gboolean
key_press (GtkWidget   *widget,
           GdkEventKey *event,
           GbSlider    *slider)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      gb_slider_set_position (slider, GB_SLIDER_NONE);
      gtk_widget_grab_focus (GTK_WIDGET (text_view));
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

int
main (int argc,
      char *argv[])
{
  GtkCssProvider *provider;
  GtkWindow *window;
  GtkHeaderBar *header_bar;
  GbSlider *slider;
  GtkButton *button;

  gtk_init (&argc, &argv);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, CSS_DATA, -1, NULL);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "title", "Slider Test",
                         "default-width", 1280,
                         "default-height", 720,
                         NULL);

  header_bar = g_object_new (GTK_TYPE_HEADER_BAR,
                             "show-close-button", TRUE,
                             "visible", TRUE,
                             NULL);
  gtk_window_set_titlebar (window, GTK_WIDGET (header_bar));

  slider = g_object_new (GB_TYPE_SLIDER,
                         "visible", TRUE,
                         NULL);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (slider));

  button = g_object_new (GTK_TYPE_BUTTON,
                         "label", "Toggle",
                         "visible", TRUE,
                         NULL);
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (clicked_cb),
                    slider);
  gtk_container_add_with_properties (GTK_CONTAINER (header_bar), GTK_WIDGET (button),
                                     "pack-type", GTK_PACK_START,
                                     NULL);

  text_view = g_object_new (GTK_TYPE_TEXT_VIEW,
                            "visible", TRUE,
                            NULL);
  gtk_container_add (GTK_CONTAINER (slider), GTK_WIDGET (text_view));

  entry = g_object_new (GTK_TYPE_ENTRY,
                        "visible", TRUE,
                        NULL);
  g_signal_connect (entry, "key-press-event", G_CALLBACK (key_press), slider);
  gtk_container_add_with_properties (GTK_CONTAINER (slider), GTK_WIDGET (entry),
                                     "position", GB_SLIDER_BOTTOM,
                                     NULL);

  gtk_window_present (window);
  g_signal_connect (window, "delete-event", gtk_main_quit, NULL);
  gtk_main ();

  return 0;
}
