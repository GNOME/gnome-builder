/* gb-application-actions.c
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

#define G_LOG_DOMAIN "gb-application"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "gb-application-actions.h"
#include "gb-application-credits.h"
#include "gb-application-private.h"
#include "gb-new-project-dialog.h"
#include "gb-support.h"
#include "gb-workbench.h"

static void
gb_application_actions_preferences (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GbApplication *self = user_data;

  IDE_ENTRY;

  g_assert (GB_IS_APPLICATION (self));

  if (self->preferences_window == NULL)
    {
      GbPreferencesWindow *window;

      window = g_object_new (GB_TYPE_PREFERENCES_WINDOW,
                             "type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
                             "window-position", GTK_WIN_POS_CENTER,
                             NULL);
      ide_set_weak_pointer (&self->preferences_window, window);
    }

  gtk_window_present (GTK_WINDOW (self->preferences_window));

  IDE_EXIT;
}

static void
gb_application_actions_support (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  GbApplication *self = user_data;
  GtkWidget *dialog;
  gchar *text = NULL;
  GList *windows;
  GError *error = NULL;
  gchar *str = NULL;
  gchar *log_path = NULL;
  gchar *name = NULL;

  name = g_strdup_printf ("gnome-builder-%u.log", (int)getpid ());
  log_path = g_build_filename (g_get_home_dir (), name, NULL);
  g_free (name);

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  str = gb_get_support_log ();

  if (!g_file_set_contents (log_path, str, -1, &error))
    {
      g_printerr ("%s\n", error->message);
      goto cleanup;
    }

  text = g_strdup_printf (_("The support log file has been written to '%s'. "
                            "Please provide this file as an attachment on "
                            "your bug report or support request."),
                            log_path);

  g_message ("%s", text);

  dialog = gtk_message_dialog_new (windows ? windows->data : NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "%s", text);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_window_present (GTK_WINDOW (dialog));

cleanup:
  g_free (text);
  g_clear_error (&error);
  g_free (str);
  g_free (log_path);
}

static void
gb_application_actions_quit (GSimpleAction *action,
                             GVariant      *param,
                             gpointer       user_data)
{
  GbApplication *self = user_data;

  IDE_ENTRY;

  g_assert (GB_IS_APPLICATION (self));

  g_application_quit (G_APPLICATION (self));

  IDE_EXIT;
}

static void
gb_application_actions_about (GSimpleAction *action,
                              GVariant      *param,
                              gpointer       user_data)
{
  GbApplication *self = user_data;
  GtkDialog *dialog;
  GtkWindow *parent = NULL;
  GList *iter;
  GList *windows;

  g_assert (GB_IS_APPLICATION (self));

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (iter = windows; iter; iter = iter->next)
    {
      if (GB_IS_WORKBENCH (iter->data))
        {
          parent = iter->data;
          break;
        }
    }

  dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                         "artists", gb_application_credits_artists,
                         "authors", gb_application_credits_authors,
                         "comments", _("An IDE for GNOME"),
                         "documenters", gb_application_credits_documenters,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", "builder",
                         "modal", FALSE,
                         "program-name", _("GNOME Builder"),
                         "transient-for", parent,
                         "translator-credits", _("translator-credits"),
                         "version", PACKAGE_VERSION,
                         "website", "https://wiki.gnome.org/Apps/Builder",
                         "website-label", _("Learn more about GNOME Builder"),
                         NULL);
  gtk_about_dialog_add_credit_section (GTK_ABOUT_DIALOG (dialog),
                                       _("Funded By"),
                                       gb_application_credits_funders);

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
gb_application_actions_open_project (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  GbApplication *self = user_data;

  g_assert (GB_IS_APPLICATION (self));

  gb_application_show_projects_window (self);
}

static void
gb_application_actions__window_open_project (GbApplication      *self,
                                             GFile              *project_file,
                                             GbNewProjectDialog *window)
{
  g_assert (GB_IS_APPLICATION (self));
  g_assert (G_IS_FILE (project_file));
  g_assert (GB_IS_NEW_PROJECT_DIALOG (window));

  gb_application_open_project (self, project_file, NULL);
  gtk_widget_destroy (GTK_WIDGET (window));
}

static void
gb_application_actions_new_project (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  GbApplication *self = user_data;
  GtkWindow *window;

  g_assert (GB_IS_APPLICATION (self));

  window = g_object_new (GB_TYPE_NEW_PROJECT_DIALOG,
                         "type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
                         "window-position", GTK_WIN_POS_CENTER,
                         NULL);

  g_signal_connect_object (window,
                           "open-project",
                           G_CALLBACK (gb_application_actions__window_open_project),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_window_present (window);
}

static const GActionEntry GbApplicationActions[] = {
  { "about",        gb_application_actions_about },
  { "open-project", gb_application_actions_open_project },
  { "new-project",  gb_application_actions_new_project },
  { "preferences",  gb_application_actions_preferences },
  { "quit",         gb_application_actions_quit },
  { "support",      gb_application_actions_support },
};

void
gb_application_actions_init (GbApplication *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self), GbApplicationActions,
                                   G_N_ELEMENTS (GbApplicationActions), self);
}
