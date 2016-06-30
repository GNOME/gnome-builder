#include <egg-file-chooser-entry.h>

gint
main (gint       argc,
      gchar *argv[])
{
  static const gchar *bool_properties[] = {
    "local-only",
    "create-folders",
    "do-overwrite-confirmation",
    "show-hidden",
    NULL
  };
  static const gchar *int_properties[] = {
    "max-width-chars",
    NULL,
  };
  GtkWindow *window;
  GtkBox *box;
  GtkBox *vbox;
  EggFileChooserEntry *entry;
  GFile *file;
  guint i;

  gtk_init (&argc, &argv);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "title", "Test EggFileChooserEntry",
                         "border-width", 24,
                         NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "valign", GTK_ALIGN_CENTER,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 36,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (box));

  vbox = g_object_new (GTK_TYPE_BOX,
                       "orientation", GTK_ORIENTATION_VERTICAL,
                       "halign", GTK_ALIGN_START,
                       "visible", TRUE,
                       "spacing", 6,
                       NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (vbox));

  entry = g_object_new (EGG_TYPE_FILE_CHOOSER_ENTRY,
                        "title", "Select a Folder",
                        "action", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                        "valign", GTK_ALIGN_CENTER,
                        "visible", TRUE,
                        NULL);

  for (i = 0; bool_properties [i]; i++)
    {
      GtkCheckButton *button;

      button = g_object_new (GTK_TYPE_CHECK_BUTTON,
                             "label", bool_properties[i],
                             "visible", TRUE,
                             "halign", GTK_ALIGN_START,
                             NULL);
      g_object_bind_property (button, "active", entry, bool_properties[i], G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (button));
    }

  for (i = 0; int_properties [i]; i++)
    {
      GtkAdjustment *adj;
      GtkSpinButton *button;
      GParamSpec *pspec;

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (entry), int_properties [i]);
      adj = g_object_new (GTK_TYPE_ADJUSTMENT,
                          "lower", (gdouble)((GParamSpecInt*)pspec)->minimum,
                          "upper", (gdouble)((GParamSpecInt*)pspec)->maximum,
                          "value", (gdouble)((GParamSpecInt*)pspec)->default_value,
                          "step-increment", 1.0,
                          NULL);
      button = g_object_new (GTK_TYPE_SPIN_BUTTON,
                             "adjustment", adj,
                             "visible", TRUE,
                             "halign", GTK_ALIGN_START,
                             NULL);
      g_object_bind_property (button, "value", entry, int_properties[i], G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (button));
    }

  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (entry));

  file = g_file_new_for_path (g_get_home_dir ());
  egg_file_chooser_entry_set_file (entry, file);

  g_signal_connect (window, "delete-event", gtk_main_quit, NULL);
  gtk_window_present (window);

  gtk_main ();

  return 0;
}
