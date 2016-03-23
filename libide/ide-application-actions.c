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
                         "copyright", "Copyright © 2014—2016 Christian Hergert, et al.",
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
ide_application_actions_help (GSimpleAction *action,
                              GVariant      *param,
                              gpointer       user_data)
{
  IdeApplication *self = user_data;
  GtkWindow *focused_window= NULL;
  GdkScreen *screen = NULL;
  GError *err = NULL;

  g_assert (IDE_IS_APPLICATION (self));

  focused_window = gtk_application_get_active_window (GTK_APPLICATION (self));

  screen = gtk_window_get_screen (focused_window);
  gtk_show_uri (screen,
                "help:gnome-builder",
                gtk_get_current_event_time (),
                &err);
  if (err)
    {
      g_message ("Unable to open help: %s\n", err->message);
      g_error_free (err);
    }
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


static void
ide_application_actions_new_project (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeApplication *self = user_data;
  IdeWorkbench *workbench = NULL;
  const GList *list;

  g_assert (IDE_IS_APPLICATION (self));

  list = gtk_application_get_windows (GTK_APPLICATION (self));

  for (; list != NULL; list = list->next)
    {
      GtkWindow *window = list->data;

      if (IDE_IS_WORKBENCH (window))
        {
          if (ide_workbench_get_context (IDE_WORKBENCH (window)) == NULL)
            {
              workbench = IDE_WORKBENCH (window);
              break;
            }
        }
    }

  if (workbench == NULL)
    {
      workbench = g_object_new (IDE_TYPE_WORKBENCH,
                                "application", self,
                                NULL);
    }

  ide_workbench_set_visible_perspective_name (workbench, "genesis");

  gtk_window_present (GTK_WINDOW (workbench));
}

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

static void
ide_application_actions_nighthack (GSimpleAction *action,
                                   GVariant      *variant,
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
ide_application_actions_dayhack (GSimpleAction *action,
                                 GVariant      *variant,
                                 gpointer       user_data)
{
  g_autoptr(GSettings) settings = NULL;

  g_object_set (gtk_settings_get_default (),
                "gtk-application-prefer-dark-theme", FALSE,
                NULL);

  settings = g_settings_new ("org.gnome.builder.editor");
  g_settings_set_string (settings, "style-scheme-name", "builder");
}

static const GActionEntry IdeApplicationActions[] = {
  { "about",        ide_application_actions_about },
  { "dayhack",      ide_application_actions_dayhack },
  { "nighthack",    ide_application_actions_nighthack },
  { "open-project", ide_application_actions_open_project },
  { "new-project",  ide_application_actions_new_project },
  { "preferences",  ide_application_actions_preferences },
  { "quit",         ide_application_actions_quit },
  { "shortcuts",    ide_application_actions_shortcuts },
  { "help",         ide_application_actions_help },
};

void
ide_application_actions_init (IdeApplication *self)
{
  static const gchar *left[] = { "F9", NULL };
  static const gchar *right[] = { "<shift>F9", NULL };
  static const gchar *bottom[] = { "<control>F9", NULL };
  static const gchar *preferences[] = { "<control>comma", NULL };
  static const gchar *global_search[] = { "<control>period", NULL };
  static const gchar *new_file[] = { "<control>n", NULL };
  static const gchar *shortcuts[] = { "<control>F1", "<control><shift>question", NULL };
  static const gchar *help[] = { "F1", NULL };
  static const gchar *command_bar[] = { "<ctrl>Return", "<ctrl>KP_Enter", NULL };

  g_action_map_add_action_entries (G_ACTION_MAP (self), IdeApplicationActions,
                                   G_N_ELEMENTS (IdeApplicationActions), self);

  /*
   * FIXME: Once we get a new shortcuts engine, port these to that.
   */
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "dockbin.left-visible", left);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "dockbin.right-visible", right);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "dockbin.bottom-visible", bottom);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.preferences", preferences);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "perspective.global-search", global_search);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "perspective.new-file", new_file);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.shortcuts", shortcuts);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "app.help", help);
  gtk_application_set_accels_for_action (GTK_APPLICATION (self), "win.show-command-bar", command_bar);
}
