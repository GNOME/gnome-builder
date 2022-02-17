{{include "license.c"}}

#include "{{prefix}}-application.h"
#include "{{prefix}}-window.h"

struct _{{PreFix}}Application
{
  GtkApplication parent_instance;
};

G_DEFINE_TYPE ({{PreFix}}Application, {{prefix_}}_application, {{if is_adwaita}}ADW_TYPE_APPLICATION{{else}}GTK_TYPE_APPLICATION{{end}})

{{PreFix}}Application *
{{prefix_}}_application_new (gchar *application_id,
{{spaces}}                  GApplicationFlags  flags)
{
  return g_object_new ({{PREFIX}}_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       NULL);
}

static void
{{prefix_}}_application_finalize (GObject *object)
{
  {{PreFix}}Application *self = ({{PreFix}}Application *)object;

  G_OBJECT_CLASS ({{prefix_}}_application_parent_class)->finalize (object);
}

static void
{{prefix_}}_application_activate (GApplication *app)
{
  GtkWindow *window;

  /* It's good practice to check your parameters at the beginning of the
   * function. It helps catch errors early and in development instead of
   * by your users.
   */
  g_assert (GTK_IS_APPLICATION (app));

  /* Get the current window or create one if necessary. */
  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (window == NULL)
    window = g_object_new ({{PREFIX}}_TYPE_WINDOW,
                           "application", app,
                           NULL);

  /* Ask the window manager/compositor to present the window. */
  gtk_window_present (window);
}


static void
{{prefix_}}_application_class_init ({{PreFix}}ApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = {{prefix_}}_application_finalize;

  /*
   * We connect to the activate callback to create a window when the application
   * has been launched. Additionally, this callback notifies us when the user
   * tries to launch a "second instance" of the application. When they try
   * to do that, we'll just present any existing window.
   */
  app_class->activate = {{prefix_}}_application_activate;
}

static void
{{prefix_}}_application_show_about (GSimpleAction *action,
{{spaces}}                         GVariant      *parameter,
{{spaces}}                         gpointer       user_data)
{
  {{PreFix}}Application *self = {{PREFIX}}_APPLICATION (user_data);
  GtkWindow *window = NULL;
  const gchar *authors[] = {"{{author}}", NULL};

  g_return_if_fail ({{PREFIX}}_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  gtk_show_about_dialog (window,
                         "program-name", "{{name}}",
                         "authors", authors,
                         "version", "{{project_version}}",
                         NULL);
}


static void
{{prefix_}}_application_init ({{PreFix}}Application *self)
{
  g_autoptr (GSimpleAction) quit_action = g_simple_action_new ("quit", NULL);
  g_signal_connect_swapped (quit_action, "activate", G_CALLBACK (g_application_quit), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (quit_action));

  g_autoptr (GSimpleAction) about_action = g_simple_action_new ("about", NULL);
  g_signal_connect (about_action, "activate", G_CALLBACK ({{prefix_}}_application_show_about), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (about_action));

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "app.quit",
                                         (const char *[]) {
                                           "<primary>q",
                                           NULL,
                                         });
}
