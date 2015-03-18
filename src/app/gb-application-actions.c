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
#include "gb-application-private.h"
#include "gb-support.h"
#include "gb-workbench.h"

static void
gb_application_actions_preferences (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GbApplication *self = user_data;
  GbPreferencesWindow *window;
  GbWorkbench *workbench = NULL;
  GList *list;

  IDE_ENTRY;

  g_assert (GB_IS_APPLICATION (self));

  if (self->preferences_window)
    {
      gtk_window_present (GTK_WINDOW (self->preferences_window));
      return;
    }

  list = gtk_application_get_windows (GTK_APPLICATION (self));

  for (; list; list = list->next)
    if (GB_IS_WORKBENCH (list->data))
      workbench = GB_WORKBENCH (list->data);

  window = g_object_new (GB_TYPE_PREFERENCES_WINDOW,
                         "transient-for", workbench,
                         NULL);
  ide_set_weak_pointer (&self->preferences_window, window);

  gtk_window_present (GTK_WINDOW (window));

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
  const gchar *artists[] = {
    "Allan Day",
    "Hylke Bons",
    "Jakub Steiner",
    NULL };
  const gchar *authors[] = {
    "Alexander Larsson",
    "Alexandre Franke",
    "Carlos Soriano",
    "Christian Hergert",
    "Cosimo Cecchi",
    "Dimitris Zenios",
    "Fabiano Fidêncio",
    "Florian Bäuerle",
    "Florian Müllner",
    "Hashem Nasarat",
    "Hylke Bons",
    "Igor Gnatenko",
    "Jakub Steiner",
    "Jasper St. Pierre",
    "Jonathon Jongsma",
    "Mathieu Bridon",
    "Megh Parikh",
    "Michael Catanzaro",
    "Pete Travis",
    "Ray Strode",
    "Roberto Majadas",
    "Ting-Wei Lan",
    "Trinh Anh Ngoc",
    "Yosef Or Boczko",
    NULL };
  const gchar *funders[] = {
    "Aaron Hergert",
    "Christian Hergert",
    /* todo: load from crowdfunding */
    NULL };
  const gchar *documenters[] = {
   NULL };
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
                         "artists", artists,
                         "authors", authors,
                         "comments", _("An IDE for GNOME"),
                         "documenters", documenters,
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
  gtk_about_dialog_add_credit_section (GTK_ABOUT_DIALOG (dialog), _("Funded By"), funders);

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_window_present (GTK_WINDOW (dialog));
}

static const GActionEntry GbApplicationActions[] = {
  { "about",       gb_application_actions_about },
  { "preferences", gb_application_actions_preferences },
  { "quit",        gb_application_actions_quit },
  { "support",     gb_application_actions_support },
};

void
gb_application_actions_init (GbApplication *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self), GbApplicationActions,
                                   G_N_ELEMENTS (GbApplicationActions), self);
}
