/* ide-application-actions.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
#define DOCS_URI "https://builder.readthedocs.io"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-build-ident.h"
#include "ide-debug.h"
#include "ide-version.h"

#include "application/ide-application.h"
#include "application/ide-application-actions.h"
#include "application/ide-application-credits.h"
#include "application/ide-application-private.h"
#include "greeter/ide-greeter-perspective.h"
#include "keybindings/ide-shortcuts-window.h"
#include "preferences/ide-preferences-window.h"
#include "workbench/ide-workbench.h"
#include "util/ide-flatpak.h"

static void
ide_application_actions_preferences (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  IdeApplication *self = user_data;
  GtkWindow *toplevel = NULL;
  GtkWindow *window;
  GList *windows;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_APPLICATION (self));

  /* Locate a toplevel for a transient-for property, or a previous
   * preferences window to display.
   */
  windows = gtk_application_get_windows (GTK_APPLICATION (self));
  for (; windows != NULL; windows = windows->next)
    {
      GtkWindow *win = windows->data;

      if (IDE_IS_PREFERENCES_WINDOW (win))
        {
          gtk_window_present (win);
          return;
        }

      if (toplevel == NULL && IDE_IS_WORKBENCH (win))
        toplevel = win;
    }

  /* Create a new window for preferences, with enough space for
   * 2 columns of preferences. The window manager will automatically
   * maximize the window if necessary.
   */
  window = g_object_new (IDE_TYPE_PREFERENCES_WINDOW,
                         "transient-for", toplevel,
                         "default-width", 1300,
                         "default-height", 800,
                         "window-position", GTK_WIN_POS_CENTER_ON_PARENT,
                         NULL);
  gtk_application_add_window (GTK_APPLICATION (self), window);
  gtk_window_present (window);

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
  g_autoptr(GString) version = NULL;
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

  version = g_string_new (NULL);

  if (g_str_has_prefix (IDE_BUILD_TYPE, "debug"))
    g_string_append (version, IDE_BUILD_IDENTIFIER);
  else
    g_string_append (version, PACKAGE_VERSION);

  if (g_strcmp0 (IDE_BUILD_CHANNEL, "other") != 0)
    g_string_append (version, "\n" IDE_BUILD_CHANNEL);

  dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
                         "artists", ide_application_credits_artists,
                         "authors", ide_application_credits_authors,
                         "comments", _("An IDE for GNOME"),
                         "copyright", "Copyright © 2014–2017 Christian Hergert, et al.",
                         "documenters", ide_application_credits_documenters,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", "org.gnome.Builder",
                         "modal", TRUE,
                         "program-name", _("GNOME Builder"),
                         "transient-for", parent,
                         "translator-credits", _("translator-credits"),
                         "use-header-bar", TRUE,
                         "version", version->str,
                         "website", "https://wiki.gnome.org/Apps/Builder",
                         "website-label", _("Learn more about GNOME Builder"),
                         NULL);
  gtk_about_dialog_add_credit_section (GTK_ABOUT_DIALOG (dialog),
                                       _("Funded By"),
                                       ide_application_credits_funders);

  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
ide_application_actions_help_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GNetworkMonitor *monitor = (GNetworkMonitor *)object;
  g_autoptr(IdeApplication) self = user_data;
  GtkWindow *focused_window;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  focused_window = gtk_application_get_active_window (GTK_APPLICATION (self));

  /*
   * If we can reach the documentation website, prefer showing up-to-date
   * documentation from the website.
   */
  if (g_network_monitor_can_reach_finish (monitor, result, NULL))
    {
      if (gtk_show_uri_on_window (focused_window, DOCS_URI, gtk_get_current_event_time (), NULL))
        IDE_EXIT;
    }

  /*
   * We failed to reach the online site for some reason (offline, transient error, etc),
   * so instead try to load the local documentation.
   */
  if (g_file_test (PACKAGE_DOCDIR"/en/index.html", G_FILE_TEST_IS_REGULAR))
    {
      const gchar *uri;
      g_autofree gchar *real_uri = NULL;
      g_autoptr(GError) error = NULL;

      if (ide_is_flatpak ())
        uri = real_uri = ide_flatpak_get_app_path ("/share/doc/gnome-builder/en/index.html");
      else
        uri = "file://"PACKAGE_DOCDIR"/en/index.html";

      if (!gtk_show_uri_on_window (focused_window, uri, gtk_get_current_event_time (), &error))
        g_warning ("Failed to load documentation: %s", error->message);
    }

  IDE_EXIT;
}

static void
ide_application_actions_help (GSimpleAction *action,
                              GVariant      *param,
                              gpointer       user_data)
{
  IdeApplication *self = user_data;
  g_autoptr(GSocketConnectable) network_address = NULL;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_APPLICATION (self));

  /*
   * Check for access to the internet. Sadly, we cannot use
   * g_network_monitor_get_network_available() because that does not seem to
   * act correctly on some systems (Ubuntu appears to be one example). So
   * instead, we can asynchronously check if we can reach the peer first.
   */
  network_address = g_network_address_parse_uri (DOCS_URI, 443, NULL);
  g_network_monitor_can_reach_async (g_network_monitor_get_default (),
                                     network_address,
                                     NULL,
                                     ide_application_actions_help_cb,
                                     g_object_ref (self));

  IDE_EXIT;
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
ide_application_actions_load_workbench_view (IdeApplication *self,
                                             const char     *genesis_view,
                                             const char     *manifest)
{
  IdeWorkbench *workbench = NULL;
  IdePerspective *greeter;
  const GList *list;

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

  greeter = ide_workbench_get_perspective_by_name (workbench, "greeter");

  if (greeter)
    {
      ide_greeter_perspective_show_genesis_view (IDE_GREETER_PERSPECTIVE (greeter),
                                                 genesis_view, manifest);
    }

  gtk_window_present (GTK_WINDOW (workbench));
}

static void
ide_application_actions_clone (GSimpleAction *action,
                               GVariant      *variant,
                               gpointer       user_data)
{
  IdeApplication *self = user_data;

  g_assert (IDE_IS_APPLICATION (self));

  ide_application_actions_load_workbench_view (self, "IdeGitGenesisAddin", NULL);
}

static void
ide_application_actions_new_project (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeApplication *self = user_data;

  g_assert (IDE_IS_APPLICATION (self));

  ide_application_actions_load_workbench_view (self, "GbpCreateProjectGenesisAddin", NULL);
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
        {
          parent = window;
          break;
        }
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

static void
ide_application_actions_load_project (GSimpleAction *action,
                                      GVariant      *args,
                                      gpointer       user_data)
{
  IdeApplication *self = user_data;
  g_autofree gchar *filename = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (IDE_IS_APPLICATION (self));

  g_variant_get (args, "s", &filename);
  file = g_file_new_for_path (filename);

  if (!ide_application_open_project (self, file))
    {
      g_message ("unable to open project specified by path - %s", filename);
    }
}

static void
ide_application_actions_load_flatpak (GSimpleAction *action,
                                      GVariant      *args,
                                      gpointer       user_data)
{
  IdeApplication *self = user_data;
  const gchar *manifest = NULL;

  g_assert (IDE_IS_APPLICATION (self));

  manifest = g_variant_get_string (args, NULL);
  ide_application_actions_load_workbench_view (self, "GbpFlatpakGenesisAddin", manifest);
}

static const GActionEntry IdeApplicationActions[] = {
  { "about",        ide_application_actions_about },
  { "clone",        ide_application_actions_clone },
  { "dayhack",      ide_application_actions_dayhack },
  { "nighthack",    ide_application_actions_nighthack },
  { "open-project", ide_application_actions_open_project },
  { "new-project",  ide_application_actions_new_project },
  { "load-project", ide_application_actions_load_project, "s"},
  { "load-flatpak", ide_application_actions_load_flatpak, "s"},
  { "preferences",  ide_application_actions_preferences },
  { "quit",         ide_application_actions_quit },
  { "shortcuts",    ide_application_actions_shortcuts },
  { "help",         ide_application_actions_help },
};

void
ide_application_actions_init (IdeApplication *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self), IdeApplicationActions,
                                   G_N_ELEMENTS (IdeApplicationActions), self);

  ide_application_actions_update (self);
}

void
ide_application_actions_update (IdeApplication *self)
{
  g_assert (IDE_IS_APPLICATION (self));

  /* Nothing to do currently */
}
