/* ide-application-actions.c
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

#define G_LOG_DOMAIN "ide-application-actions"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-application.h"
#include "ide-application-actions.h"
#include "ide-application-credits.h"
#include "ide-application-private.h"
#include "ide-debug.h"
#include "ide-shortcuts-window.h"
#include "ide-support.h"
#include "ide-workbench.h"

static void
ide_application_actions_preferences (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  IdeApplication *self = user_data;
  GList *windows;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  /*
   * TODO: Make this work at the greeter screen too.
   */

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (; windows; windows = windows->next)
    {
      GtkWindow *window = windows->data;
      const gchar *name;

      if (!IDE_IS_WORKBENCH (window))
        continue;

      name = ide_workbench_get_visible_perspective_name (IDE_WORKBENCH (window));

      if (!ide_str_equal0 (name, "greeter"))
        {
          ide_workbench_set_visible_perspective_name (IDE_WORKBENCH (window), "preferences");
          IDE_EXIT;
        }
    }

  IDE_EXIT;
}

static void
ide_application_actions_support (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  IdeApplication *self = user_data;
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

  str = ide_get_support_log ();

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
ide_application_actions_quit (GSimpleAction *action,
                              GVariant      *param,
                              gpointer       user_data)
{
  IdeApplication *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));

  g_application_quit (G_APPLICATION (self));

  IDE_EXIT;
}

static void
ide_application_actions_about (GSimpleAction *action,
                               GVariant      *param,
                               gpointer       user_data)
{
  IdeApplication *self = user_data;
  GtkDialog *dialog;
  GtkWindow *parent = NULL;
  GList *iter;
  GList *windows;

  g_assert (IDE_IS_APPLICATION (self));

  windows = gtk_application_get_windows (GTK_APPLICATION (self));

  for (iter = windows; iter; iter = iter->next)
    {
      if (IDE_IS_WORKBENCH (iter->data))
        {
          parent = iter->data;
          break;
        }
    }

  dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                         "artists", ide_application_credits_artists,
                         "authors", ide_application_credits_authors,
                         "comments", _("An IDE for GNOME"),
                         "documenters", ide_application_credits_documenters,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", "builder",
                         "modal", FALSE,
                         "program-name", _("GNOME Builder"),
                         "transient-for", parent,
                         "translator-credits", _("translator-credits"),
                         "version", PACKAGE_VERSION,
                         "website", "https://wiki.gnome.org/Apps/Builder",
                         "website-label", _("Learn more about GNOME Builder"),
                         "window-position", GTK_WIN_POS_CENTER,
                         NULL);
  gtk_about_dialog_add_credit_section (GTK_ABOUT_DIALOG (dialog),
                                       _("Funded By"),
                                       ide_application_credits_funders);

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
ide_application_actions_open_project (GSimpleAction *action,
                                      GVariant      *variant,
                                      gpointer       user_data)
{
  IdeApplication *self = user_data;

  g_assert (IDE_IS_APPLICATION (self));

  ide_application_show_projects_window (self);
}

#if 0
static void
ide_application_actions_open_project_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeApplication *self = (IdeApplication *)object;
  g_autoptr(IdeNewProjectDialog) window = user_data;
  g_autoptr(GError) error = NULL;
  GtkWindow *transient_for;

  g_assert (IDE_IS_NEW_PROJECT_DIALOG (window));

  if (!ide_application_open_project_finish (self, result, &error))
    {
      /* todo: warning message */
      g_warning ("%s", error->message);
    }

  transient_for = gtk_window_get_transient_for (GTK_WINDOW (window));

  if (IDE_IS_GREETER_WINDOW (transient_for))
    g_object_ref (transient_for);
  else
    transient_for = NULL;

  gtk_widget_destroy (GTK_WIDGET (window));

  if (transient_for != NULL)
    {
      gtk_widget_destroy (GTK_WIDGET (transient_for));
      g_object_unref (transient_for);
    }
}

static void
ide_application_actions__window_open_project (IdeApplication      *self,
                                              GFile               *project_file,
                                              IdeNewProjectDialog *window)
{
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (G_IS_FILE (project_file));
  g_assert (IDE_IS_NEW_PROJECT_DIALOG (window));

  ide_application_open_project_async (self, project_file, NULL, NULL,
                                     ide_application_actions_open_project_cb,
                                     g_object_ref (window));
}

static void
ide_application_actions_new_project (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeApplication *self = user_data;
  GtkWindow *transient_for = NULL;
  GtkWindow *window;
  GList *windows;

  g_assert (IDE_IS_APPLICATION (self));

  for (windows = gtk_window_group_list_windows (self->greeter_group);
       windows;
       windows = windows->next)
    {
      if (IDE_IS_NEW_PROJECT_DIALOG (windows->data))
        {
          gtk_window_present (windows->data);
          goto cleanup;
        }
      else if (IDE_IS_GREETER_WINDOW (windows->data))
        {
          transient_for = windows->data;
        }
    }

  window = g_object_new (IDE_TYPE_NEW_PROJECT_DIALOG,
                         "type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
                         "transient-for", transient_for,
                         "window-position", transient_for ? GTK_WIN_POS_CENTER_ON_PARENT
                                                          : GTK_WIN_POS_CENTER,
                         NULL);

  g_signal_connect_object (window,
                           "open-project",
                           G_CALLBACK (ide_application_actions__window_open_project),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_window_group_add_window (self->greeter_group, GTK_WINDOW (window));
  gtk_window_present (window);

cleanup:
  g_list_free (windows);
}
#endif

static void
ide_application_actions_shortcuts (GSimpleAction *action,
                                   GVariant      *variant,
                                   gpointer       user_data)
{
  IdeApplication *self = user_data;
  GtkWindow *window;
  GtkWindow *parent = NULL;
  GList *list;

  g_assert (IDE_IS_APPLICATION (self));

  list = gtk_application_get_windows (GTK_APPLICATION (self));

  for (; list; list = list->next)
    {
      window = list->data;

      if (IDE_IS_SHORTCUTS_WINDOW (window))
        {
          gtk_window_present (window);
          return;
        }

      if (IDE_IS_WORKBENCH (window))
        parent = window;
    }

  window = g_object_new (IDE_TYPE_SHORTCUTS_WINDOW,
                         "application", self,
                         "window-position", GTK_WIN_POS_CENTER,
                         "transient-for", parent,
                         NULL);

  gtk_window_present (GTK_WINDOW (window));
}

static const GActionEntry IdeApplicationActions[] = {
  { "about",        ide_application_actions_about },
  { "open-project", ide_application_actions_open_project },
  //{ "new-project",  ide_application_actions_new_project },
  { "preferences",  ide_application_actions_preferences },
  { "quit",         ide_application_actions_quit },
  { "shortcuts",    ide_application_actions_shortcuts },
  { "support",      ide_application_actions_support },
};

void
ide_application_actions_init (IdeApplication *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self), IdeApplicationActions,
                                   G_N_ELEMENTS (IdeApplicationActions), self);
}
