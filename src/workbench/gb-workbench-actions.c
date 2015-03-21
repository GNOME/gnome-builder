/* gb-workbench-actions.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "gb-workbench-actions"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gb-workbench.h"
#include "gb-workbench-actions.h"
#include "gb-workbench-private.h"

static void
gb_workbench_actions__build_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(GbWorkbench) workbench = user_data;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(GError) error = NULL;
  IdeBuilder *builder = (IdeBuilder *)object;

  g_assert (GB_IS_WORKBENCH (workbench));

  build_result = ide_builder_build_finish (builder, result, &error);

  if (error)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (workbench),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Build Failure"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_window_present (GTK_WINDOW (dialog));
    }
}

static void
gb_workbench_actions_build (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  GbWorkbench *workbench = user_data;
  IdeDeviceManager *device_manager;
  IdeBuildSystem *build_system;
  IdeContext *context;
  IdeDevice *device;
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(GKeyFile) config = NULL;
  g_autoptr(GError) error = NULL;

  /*
   * TODO: We want to have the ability to choose the device we want to build for.  The simple answer
   * here is to just have a combo of sorts to choose the target device. But that is going to be left
   * to the designers to figure out the right way to go about it.
   *
   * For now, we will just automatically build with the "local" device.
   */

  g_assert (GB_IS_WORKBENCH (workbench));

  context = gb_workbench_get_context (workbench);
  device_manager = ide_context_get_device_manager (context);
  device = ide_device_manager_get_device (device_manager, "local");
  build_system = ide_context_get_build_system (context);
  config = g_key_file_new ();
  builder = ide_build_system_get_builder (build_system, config, device, &error);

  if (builder == NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (workbench),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Project build system does not support building"));
      if (error && error->message)
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", error->message);
      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_window_present (GTK_WINDOW (dialog));
      return;
    }

  ide_builder_build_async (builder,
                           NULL,
                           NULL, /* todo: cancellable */
                           gb_workbench_actions__build_cb,
                           g_object_ref (workbench));
}

static void
gb_workbench_actions_global_search (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GbWorkbench *self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->search_box));
}

static void
gb_workbench_actions_open_uri_list (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GbWorkbench *self = user_data;
  const gchar **uri_list;

  g_assert (GB_IS_WORKBENCH (self));

  uri_list = g_variant_get_strv (parameter, NULL);

  if (uri_list != NULL)
    {
      gb_workbench_open_uri_list (self, (const gchar * const *)uri_list);
      g_free (uri_list);
    }
}

static void
gb_workbench_actions_open_response (GtkFileChooser *chooser,
                                    gint            response_id,
                                    gpointer        user_data)
{
  g_autoptr(GbWorkbench) self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gtk_widget_hide (GTK_WIDGET (chooser));

  switch (response_id)
    {
    case GTK_RESPONSE_OK:
      {
        GSList *files;
        GSList *iter;
        gchar *file_uri;
        gchar *uri;

        file_uri = gtk_file_chooser_get_uri (chooser);
        uri = g_path_get_dirname (file_uri);
        if (g_strcmp0 (self->current_folder_uri, uri) != 0)
          {
            g_free (self->current_folder_uri);
            self->current_folder_uri = uri;
            uri = NULL;
          }
        g_free (uri);
        g_free (file_uri);

        files = gtk_file_chooser_get_files (chooser);
        for (iter = files; iter; iter = iter->next)
          {
            gb_workbench_open (self, G_FILE (iter->data));
            g_clear_object (&iter->data);
          }
        g_slist_free (files);
      }
      break;

    case GTK_RESPONSE_CANCEL:
    default:
      break;
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
gb_workbench_actions_open (GSimpleAction *action,
                           GVariant      *param,
                           gpointer       user_data)
{
  GbWorkbench *self = user_data;
  GtkDialog *dialog;
  GtkWidget *suggested;

  g_assert (GB_IS_WORKBENCH (self));

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                         "local-only", FALSE,
                         "modal", TRUE,
                         "select-multiple", TRUE,
                         "show-hidden", FALSE,
                         "transient-for", self,
                         "title", _("Open Document"),
                         NULL);

  if (self->current_folder_uri != NULL)
    gtk_file_chooser_set_current_folder_uri  (GTK_FILE_CHOOSER (dialog), self->current_folder_uri);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Open"), GTK_RESPONSE_OK,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  suggested = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (suggested),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (gb_workbench_actions_open_response),
                    g_object_ref (self));

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
gb_workbench_actions_save_all (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
}

static void
gb_workbench_actions_show_command_bar (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
  GbWorkbench *self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gb_command_bar_show (self->command_bar);
}

static void
gb_workbench_actions_nighthack (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  g_autoptr(GSettings) settings = NULL;

  g_object_set (gtk_settings_get_default (),
                "gtk-application-prefer-dark-theme", TRUE,
                NULL);

  settings = g_settings_new ("org.gnome.builder.editor");
  g_settings_set_string (settings, "style-scheme-name", "builder-dark");
}

static void
gb_workbench_actions_dayhack (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  g_autoptr(GSettings) settings = NULL;

  g_object_set (gtk_settings_get_default (),
                "gtk-application-prefer-dark-theme", FALSE,
                NULL);

  settings = g_settings_new ("org.gnome.builder.editor");
  g_settings_set_string (settings, "style-scheme-name", "builder");
}

static const GActionEntry GbWorkbenchActions[] = {
  { "build",            gb_workbench_actions_build },
  { "dayhack",          gb_workbench_actions_dayhack },
  { "global-search",    gb_workbench_actions_global_search },
  { "nighthack",        gb_workbench_actions_nighthack },
  { "open",             gb_workbench_actions_open },
  { "open-uri-list",    gb_workbench_actions_open_uri_list, "as" },
  { "save-all",         gb_workbench_actions_save_all },
  { "show-command-bar", gb_workbench_actions_show_command_bar },
};

void
gb_workbench_actions_init (GbWorkbench *self)
{
  GSimpleActionGroup *actions;

  g_assert (GB_IS_WORKBENCH (self));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), GbWorkbenchActions,
                                   G_N_ELEMENTS (GbWorkbenchActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "workbench", G_ACTION_GROUP (actions));
}
