{{include "license.c"}}

#include "config.h"

#include "{{prefix}}-application.h"
#include "{{prefix}}-window.h"

struct _{{PreFix}}Application
{
{{if is_adwaita}}
  AdwApplication parent_instance;
{{else}}
  GtkApplication parent_instance;
{{end}}
};

G_DEFINE_TYPE ({{PreFix}}Application, {{prefix_}}_application, {{if is_adwaita}}ADW_TYPE_APPLICATION{{else}}GTK_TYPE_APPLICATION{{end}})

{{PreFix}}Application *
{{prefix_}}_application_new (const char        *application_id,
{{spaces}}                  GApplicationFlags  flags)
{
  g_return_val_if_fail (application_id != NULL, NULL);

  return g_object_new ({{PREFIX}}_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       NULL);
}

static void
{{prefix_}}_application_activate (GApplication *app)
{
  GtkWindow *window;

  g_assert ({{PREFIX}}_IS_APPLICATION (app));

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (window == NULL)
    window = g_object_new ({{PREFIX}}_TYPE_WINDOW,
                           "application", app,
                           NULL);

  gtk_window_present (window);
}

static void
{{prefix_}}_application_class_init ({{PreFix}}ApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  app_class->activate = {{prefix_}}_application_activate;
}

static void
{{prefix_}}_application_about_action (GSimpleAction *action,
{{spaces}}                           GVariant      *parameter,
{{spaces}}                           gpointer       user_data)
{
  static const char *authors[] = {"{{author}}", NULL};
  {{PreFix}}Application *self = user_data;
  GtkWindow *window = NULL;

  g_assert ({{PREFIX}}_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  gtk_show_about_dialog (window,
                         "program-name", "{{name}}",
                         "authors", authors,
                         "version", "{{project_version}}",
                         NULL);
}

static void
{{prefix_}}_application_quit_action (GSimpleAction *action,
{{spaces}}                          GVariant      *parameter,
{{spaces}}                          gpointer       user_data)
{
  {{PreFix}}Application *self = user_data;

  g_assert ({{PREFIX}}_IS_APPLICATION (self));

  g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
  { "quit", {{prefix_}}_application_quit_action },
  { "about", {{prefix_}}_application_about_action },
};

static void
{{prefix_}}_application_init ({{PreFix}}Application *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   app_actions,
                                   G_N_ELEMENTS (app_actions),
                                   self);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "app.quit",
                                         (const char *[]) { "<primary>q", NULL });
}
